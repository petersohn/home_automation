#include "SignatureVerifyInterface.hpp"

#include <utility>

// Portable state machine exposing signed-envelope verification through the
// Interface/Actions contract. Scheduling follows the contract documented in
// the header: at most one action per update, in priority order. The coalesced
// overflow notification reuses the pending event slot: it is enqueued when a
// verification result is presented, so it is emitted on the following update,
// right after that result and before any queued error presentation or command
// processing. This keeps the pending notification unstarvable while allowing
// the command that was queued at overflow time to be processed first.

namespace {

std::vector<std::string> singleValue(const std::string& value) {
    return std::vector<std::string>{value};
}

}  // namespace

SignatureVerifyInterface::SignatureVerifyInterface(
    std::unique_ptr<SignatureVerifier> verifier)
    : verifier(std::move(verifier)) {}

void SignatureVerifyInterface::start() {
    // Nothing to do: the initial empty event is emitted by the first update
    // through the normal interface/action path, exactly once.
}

void SignatureVerifyInterface::execute(const std::string& command) {
    // Oversized commands are rejected before any copy: only the error code
    // is queued, the command slot stays untouched.
    if (command.size() > maxCommandSize) {
        this->oversizedQueued = true;
        return;
    }
    // A single command slot: an occupied slot makes the new arrival overflow
    // into one coalesced busy notification instead of queueing.
    if (this->hasPendingCommand) {
        this->busyErrorQueued = true;
        return;
    }
    this->pendingCommand = command;
    this->hasPendingCommand = true;
}

void SignatureVerifyInterface::update(Actions action) {
    // Priority 1: the initial empty event is emitted exactly once, even when
    // a command or an error is already queued; those are handled on the next
    // updates.
    if (this->stage == Stage::initialPending) {
        this->stage = Stage::idle;
        action.fire(singleValue(""));
        return;
    }
    // Priority 2: a pending verification result is presented and can never be
    // starved. Nothing else happens in this update.
    if (this->pendingEvent != PendingEvent::none) {
        // The presentation is built from the pending slots before they are
        // cleared, so it reflects the result being presented, never the
        // coalesced busy notification staged below for the next update.
        const std::vector<std::string> presentation =
            this->pendingPresentation();
        this->pendingEvent = PendingEvent::none;
        this->pendingError = PendingError::none;
        this->stage = Stage::idle;
        if (this->busyErrorQueued) {
            // An overflow was observed while the presented result's command
            // was queued: the coalesced busy notification becomes the next
            // pending event, so it is emitted after this result and before
            // any further command processing.
            this->busyErrorQueued = false;
            this->pendingEvent = PendingEvent::error;
            this->pendingError = PendingError::busy;
        }
        action.fire(presentation);
        return;
    }
    // Priority 3: queued error presentations, one per update. The oversized
    // input error is emitted before any command processing; the coalesced
    // busy overflow notification is handled through the pending event slot
    // above, right after the result it follows.
    if (this->oversizedQueued) {
        this->oversizedQueued = false;
        action.fire(singleValue("ERROR command too large"));
        return;
    }
    // Priority 4: at most one queued command per update is decoded and
    // verified; its result becomes the pending event presented on a later
    // update. Idle updates never repeat an earlier event.
    if (this->hasPendingCommand) {
        const std::string command = std::move(this->pendingCommand);
        this->pendingCommand.clear();
        this->hasPendingCommand = false;
        this->processPendingCommand(command);
    }
}

void SignatureVerifyInterface::processPendingCommand(
    const std::string& command) {
    SignedEnvelope envelope;
    const EnvelopeError decodeError = decodeSignedEnvelope(command, envelope);
    if (decodeError != EnvelopeError::none) {
        // A malformed envelope never reaches the verifier.
        this->pendingEvent = PendingEvent::error;
        this->pendingError = errorFor(decodeError);
        this->stage = Stage::busy;
        return;
    }
    const SignatureResult result = this->verifier->verify(
        SignatureBytes{envelope.data.data(), envelope.data.size()},
        SignatureBytes{envelope.signature.data(), envelope.signature.size()});
    this->stage = Stage::busy;
    switch (result.status) {
    case SignatureStatus::valid:
        this->pendingEvent = PendingEvent::ok;
        this->pendingError = PendingError::none;
        return;
    case SignatureStatus::invalid:
        this->pendingEvent = PendingEvent::fail;
        this->pendingError = PendingError::none;
        return;
    case SignatureStatus::error:
    default:
        this->pendingEvent = PendingEvent::error;
        this->pendingError = errorFor(result.error);
        return;
    }
}

std::vector<std::string> SignatureVerifyInterface::pendingPresentation() const {
    switch (this->pendingEvent) {
    case PendingEvent::ok:
        return singleValue("OK");
    case PendingEvent::fail:
        return singleValue("FAIL invalid signature");
    case PendingEvent::error:
        return singleValue(
            std::string("ERROR ") + errorMessage(this->pendingError));
    case PendingEvent::none:
        break;
    }
    return singleValue("ERROR internal error");
}

const char* SignatureVerifyInterface::errorMessage(PendingError error) {
    switch (error) {
    case PendingError::invalidJson:
        return "invalid JSON";
    case PendingError::invalidFields:
        return "invalid fields";
    case PendingError::invalidBase64:
        return "invalid base64";
    case PendingError::commandTooLarge:
        return "command too large";
    case PendingError::dataTooLarge:
        return "data too large";
    case PendingError::invalidSignatureLength:
        return "invalid signature length";
    case PendingError::invalidCertificate:
        return "invalid certificate";
    case PendingError::unsupportedKey:
        return "unsupported key";
    case PendingError::certificateUnavailable:
        return "certificate unavailable";
    case PendingError::internalError:
        return "internal error";
    case PendingError::busy:
        return "busy";
    case PendingError::none:
        break;
    }
    return "internal error";
}

SignatureVerifyInterface::PendingError SignatureVerifyInterface::errorFor(
    EnvelopeError error) {
    switch (error) {
    case EnvelopeError::invalidJson:
        return PendingError::invalidJson;
    case EnvelopeError::invalidFields:
        return PendingError::invalidFields;
    case EnvelopeError::invalidBase64:
        return PendingError::invalidBase64;
    case EnvelopeError::commandTooLarge:
        return PendingError::commandTooLarge;
    case EnvelopeError::dataTooLarge:
        return PendingError::dataTooLarge;
    case EnvelopeError::invalidSignatureLength:
        return PendingError::invalidSignatureLength;
    case EnvelopeError::none:
        break;
    }
    return PendingError::internalError;
}

SignatureVerifyInterface::PendingError SignatureVerifyInterface::errorFor(
    SignatureError error) {
    switch (error) {
    case SignatureError::none:
        return PendingError::none;
    case SignatureError::invalidCertificate:
        return PendingError::invalidCertificate;
    case SignatureError::unsupportedKey:
        return PendingError::unsupportedKey;
    case SignatureError::certificateUnavailable:
        return PendingError::certificateUnavailable;
    case SignatureError::invalidSignatureLength:
        return PendingError::invalidSignatureLength;
    case SignatureError::internalError:
        return PendingError::internalError;
    case SignatureError::invalidSignature:
        break;
    }
    return PendingError::internalError;
}