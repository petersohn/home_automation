#ifndef COMMON_SIGNATUREVERIFYINTERFACE_HPP
#define COMMON_SIGNATUREVERIFYINTERFACE_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "Interface.hpp"
#include "SignatureVerifier.hpp"
#include "SignedEnvelope.hpp"

// Portable interface adapter that exposes signed-envelope verification
// through the standard Interface/Actions state machine contract. It owns its
// verifier, so nothing else does; no MQTT, JSON, or device dependencies
// appear here. Envelope decoding is performed inside the interface and a
// malformed envelope never reaches the verifier. Verifier results and
// decoding errors are mapped to stable presentation strings only in this
// layer, including the "ERROR " prefix; verifier implementations stay
// structured.
//
// Scheduling contract (at most one of the following happens per update):
//
// 1. If the initial empty event was not emitted yet, the single value {""} is
//    emitted and the update ends. A command queued before that update is
//    processed on the next update. The initial event is emitted exactly once.
// 2. Otherwise, if a verification result is pending (the interface is
//    busy), it is emitted and the update ends. This pending notification
//    cannot be starved by later arrivals.
// 3. Otherwise queued error presentations are emitted, one per update. The
//    coalesced overflow "ERROR busy" is enqueued into the pending event slot
//    right after a verification result is presented, so it goes out through
//    rule 2 on the following update, before any further command processing.
//    The oversized-input error is emitted here, once, before any command
//    processing.
// 4. Otherwise, if a command is queued, exactly one command is dequeued,
//    decoded, and verified; the result becomes the pending event emitted on
//    a later update. At most one verification runs in a single update.
//
// execute() keeps a single command slot. When the slot is already occupied
// the command is dropped and a single coalesced "ERROR busy" event is
// scheduled to be emitted after the currently pending result. Oversized
// commands (longer than maxCommandSize bytes) are rejected in execute()
// without copying the payload: only an error code is queued.
//
// Pending state is fixed size: one command slot, one pending event slot, and
// two coalescing flags. No growable pending container exists, so arrivals
// while busy can never accumulate without bound; drops are coalesced into
// the busy error instead.
//
// Presentation strings (stable contract):
//   valid                            -> "OK"
//   invalid signature                -> "FAIL invalid signature"
//   invalid JSON                     -> "ERROR invalid JSON"
//   invalid fields                   -> "ERROR invalid fields"
//   invalid base64                   -> "ERROR invalid base64"
//   command too large                -> "ERROR command too large"
//   data too large                   -> "ERROR data too large"
//   invalid signature length         -> "ERROR invalid signature length"
//   verifier invalid certificate     -> "ERROR invalid certificate"
//   verifier unsupported key         -> "ERROR unsupported key"
//   verifier certificate unavailable -> "ERROR certificate unavailable"
//   verifier internal error          -> "ERROR internal error"
//   overflow while busy              -> "ERROR busy"
class SignatureVerifyInterface : public Interface {
public:
    explicit SignatureVerifyInterface(
        std::unique_ptr<SignatureVerifier> verifier);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

    ~SignatureVerifyInterface() override = default;

private:
    // Commands longer than this are rejected in execute() without copying.
    static constexpr std::size_t maxCommandSize = 512;

    enum class Stage { initialPending, idle, busy };
    enum class PendingEvent { none, ok, fail, error };
    enum class PendingError {
        none,
        invalidJson,
        invalidFields,
        invalidBase64,
        commandTooLarge,
        dataTooLarge,
        invalidSignatureLength,
        invalidCertificate,
        unsupportedKey,
        certificateUnavailable,
        internalError,
        // Coalesced overflow marker, not produced by decoding or the
        // verifier: it is enqueued into the pending event slot right after a
        // verification result is presented and is presented as "ERROR busy".
        busy
    };

    Stage stage = Stage::initialPending;
    PendingEvent pendingEvent = PendingEvent::none;
    PendingError pendingError = PendingError::none;
    std::unique_ptr<SignatureVerifier> verifier;

    // Single command slot: an occupied slot makes a new arrival overflow
    // instead of queueing.
    std::string pendingCommand;
    bool hasPendingCommand = false;

    // Coalescing error flags: further arrivals of the same condition keep
    // the flag set instead of queueing more events.
    bool busyErrorQueued = false;
    bool oversizedQueued = false;

    // Decodes and verifies at most one queued command, turning the outcome
    // into the pending event presented on a following update.
    void processPendingCommand(const std::string& command);

    // Stable presentation values for the pending event, including the
    // "ERROR " prefix; verifier and envelope layers stay structured.
    std::vector<std::string> pendingPresentation() const;

    // Error-code message mapping, shared by the presentation builder.
    static const char* errorMessage(PendingError error);
    static PendingError errorFor(EnvelopeError error);
    static PendingError errorFor(SignatureError error);
};

#endif  // COMMON_SIGNATUREVERIFYINTERFACE_HPP