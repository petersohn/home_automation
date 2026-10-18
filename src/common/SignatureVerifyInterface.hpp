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
// Scheduling contract:
//
// The first update emits the initial empty value exactly once. On each later
// update, the interface either reports one rejected arrival burst or decodes,
// verifies, and emits the result for its single queued command in that same
// update. Multiple execute() calls between updates discard every command in
// that burst and produce one "ERROR busy" event without verification.
// Oversized commands are rejected before copying; a single oversized arrival
// produces "ERROR command too large", while any multiple-arrival burst is
// reported as busy regardless of command sizes. Arrivals after a busy burst
// remain discarded until the next update clears it.
//
// Pending memory is bounded to one command of at most maxCommandSize bytes
// and a few flags; discarded commands are never retained.
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

    enum class Error {
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
        internalError
    };

    bool initialEventPending = true;
    std::unique_ptr<SignatureVerifier> verifier;

    // A burst holds at most one bounded command. A second arrival discards it
    // and marks the whole burst busy until update().
    std::string pendingCommand;
    bool hasPendingCommand = false;
    bool oversizedPending = false;
    bool busyPending = false;

    // Decodes and verifies one command, returning its stable presentation.
    std::vector<std::string> processCommand(const std::string& command);

    static const char* errorMessage(Error error);
    static Error errorFor(EnvelopeError error);
    static Error errorFor(SignatureError error);
};

#endif  // COMMON_SIGNATUREVERIFYINTERFACE_HPP
