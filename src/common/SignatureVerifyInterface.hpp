#ifndef COMMON_SIGNATUREVERIFYINTERFACE_HPP
#define COMMON_SIGNATUREVERIFYINTERFACE_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "Interface.hpp"
#include "SignatureVerifier.hpp"
#include "SignedEnvelope.hpp"

/**
 * Adapts signed-envelope verification to Interface actions and owns its
 * verifier.
 * Envelope decoding precedes verification; invalid envelopes never reach the
 * verifier. The first update emits one empty value; later updates emit queued
 * command results. Multiple arrivals before an update discard the burst
 * and emit "ERROR busy"; further arrivals are dropped until that update.
 * Oversized commands are rejected before copying, and pending storage is
 * bounded to 512 bytes.
 *
 * Results are "OK", "FAIL invalid signature", or "ERROR <message>" where
 * <message> is invalid JSON, invalid fields, invalid base64, command too large,
 * data too large, invalid signature length, invalid certificate, unsupported
 * key, certificate unavailable, internal error, or busy.
 */
class SignatureVerifyInterface : public Interface {
public:
    explicit SignatureVerifyInterface(
        std::unique_ptr<SignatureVerifier> verifier);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

    ~SignatureVerifyInterface() override = default;

private:
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

    std::string pendingCommand;
    bool hasPendingCommand = false;
    bool oversizedPending = false;
    bool busyPending = false;

    std::vector<std::string> processCommand(const std::string& command);

    static const char* errorMessage(Error error);
    static Error errorFor(EnvelopeError error);
    static Error errorFor(SignatureError error);
};

#endif  // COMMON_SIGNATUREVERIFYINTERFACE_HPP
