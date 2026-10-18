#include "SignatureVerifyInterface.hpp"

#include <utility>

namespace {

std::vector<std::string> singleValue(const std::string& value) {
    return std::vector<std::string>{value};
}

}  // namespace

SignatureVerifyInterface::SignatureVerifyInterface(
    std::unique_ptr<SignatureVerifier> verifier)
    : verifier(std::move(verifier)) {}

void SignatureVerifyInterface::start() {}

void SignatureVerifyInterface::execute(const std::string& command) {
    if (this->busyPending) {
        return;
    }

    if (this->hasPendingCommand || this->oversizedPending) {
        // A second arrival makes the burst busy regardless of command size.
        this->pendingCommand.clear();
        this->hasPendingCommand = false;
        this->oversizedPending = false;
        this->busyPending = true;
        return;
    }

    if (command.size() > maxCommandSize) {
        this->oversizedPending = true;
        return;
    }

    this->pendingCommand = command;
    this->hasPendingCommand = true;
}

void SignatureVerifyInterface::update(Actions action) {
    if (this->initialEventPending) {
        this->initialEventPending = false;
        action.fire(singleValue(""));
        return;
    }

    if (this->busyPending) {
        this->busyPending = false;
        action.fire(singleValue("ERROR busy"));
        return;
    }

    if (this->oversizedPending) {
        this->oversizedPending = false;
        action.fire(singleValue("ERROR command too large"));
        return;
    }

    if (this->hasPendingCommand) {
        std::string command = std::move(this->pendingCommand);
        this->pendingCommand.clear();
        this->hasPendingCommand = false;
        action.fire(this->processCommand(command));
    }
}

std::vector<std::string> SignatureVerifyInterface::processCommand(
    const std::string& command) {
    SignedEnvelope envelope;
    const EnvelopeError decodeError = decodeSignedEnvelope(command, envelope);
    if (decodeError != EnvelopeError::none) {
        return singleValue(
            std::string("ERROR ") + errorMessage(errorFor(decodeError)));
    }

    const SignatureResult result = this->verifier->verify(
        SignatureBytes{envelope.data.data(), envelope.data.size()},
        SignatureBytes{envelope.signature.data(), envelope.signature.size()});
    switch (result.status) {
    case SignatureStatus::valid:
        return singleValue("OK");
    case SignatureStatus::invalid:
        return singleValue("FAIL invalid signature");
    case SignatureStatus::error:
    default:
        return singleValue(
            std::string("ERROR ") + errorMessage(errorFor(result.error)));
    }
}

const char* SignatureVerifyInterface::errorMessage(Error error) {
    switch (error) {
    case Error::invalidJson:
        return "invalid JSON";
    case Error::invalidFields:
        return "invalid fields";
    case Error::invalidBase64:
        return "invalid base64";
    case Error::commandTooLarge:
        return "command too large";
    case Error::dataTooLarge:
        return "data too large";
    case Error::invalidSignatureLength:
        return "invalid signature length";
    case Error::invalidCertificate:
        return "invalid certificate";
    case Error::unsupportedKey:
        return "unsupported key";
    case Error::certificateUnavailable:
        return "certificate unavailable";
    case Error::internalError:
    case Error::none:
        return "internal error";
    }
    return "internal error";
}

SignatureVerifyInterface::Error SignatureVerifyInterface::errorFor(
    EnvelopeError error) {
    switch (error) {
    case EnvelopeError::invalidJson:
        return Error::invalidJson;
    case EnvelopeError::invalidFields:
        return Error::invalidFields;
    case EnvelopeError::invalidBase64:
        return Error::invalidBase64;
    case EnvelopeError::commandTooLarge:
        return Error::commandTooLarge;
    case EnvelopeError::dataTooLarge:
        return Error::dataTooLarge;
    case EnvelopeError::invalidSignatureLength:
        return Error::invalidSignatureLength;
    case EnvelopeError::none:
        break;
    }
    return Error::internalError;
}

SignatureVerifyInterface::Error SignatureVerifyInterface::errorFor(
    SignatureError error) {
    switch (error) {
    case SignatureError::none:
        return Error::none;
    case SignatureError::invalidCertificate:
        return Error::invalidCertificate;
    case SignatureError::unsupportedKey:
        return Error::unsupportedKey;
    case SignatureError::certificateUnavailable:
        return Error::certificateUnavailable;
    case SignatureError::invalidSignatureLength:
        return Error::invalidSignatureLength;
    case SignatureError::internalError:
        return Error::internalError;
    case SignatureError::invalidSignature:
        break;
    }
    return Error::internalError;
}
