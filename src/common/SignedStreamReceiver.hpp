#ifndef COMMON_SIGNEDSTREAMRECEIVER_HPP
#define COMMON_SIGNEDSTREAMRECEIVER_HPP

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "MqttTransportState.hpp"
#include "SignatureVerifier.hpp"
#include "StreamCommand.hpp"

class EspApi;

struct SignedStreamReply {
    std::string status;
    uint32_t transferred;
    std::string hash;
};

class SignedStreamSink {
public:
    virtual ~SignedStreamSink() = default;

    virtual bool begin(uint32_t size) = 0;
    virtual bool append(SignatureBytes data) = 0;
    virtual std::string complete(const SignedStreamReply& verified) = 0;
    virtual void abort() = 0;
};

class SignedStreamReceiver {
public:
    SignedStreamReceiver(
        EspApi& esp, std::unique_ptr<SignatureVerifier> verifier,
        SignedStreamSink& sink,
        std::function<MqttTransportState()> transportState);

    void execute(const std::string& command);
    std::optional<SignedStreamReply> update();

private:
    static constexpr std::size_t maxCommandSize = 512;
    static constexpr uint32_t inactivityTimeoutMs = 30000;

    enum class Error {
        invalidJson,
        invalidFields,
        invalidHash,
        invalidBase64,
        commandTooLarge,
        dataTooLarge,
        invalidSignatureLength,
        busy,
        conflictingSize,
        invalidOffset,
        transferMismatch,
        noActiveTransfer,
        incompleteTransfer,
        timeout,
        disconnected,
        invalidCertificate,
        unsupportedKey,
        certificateUnavailable,
        invalidState,
        internalError
    };

    EspApi& esp;
    std::unique_ptr<SignatureVerifier> verifier;
    SignedStreamSink& sink;
    std::function<MqttTransportState()> transportState;

    bool transportInitialized = false;
    MqttTransportState observedTransport{false, 0};
    bool transferActive = false;
    std::array<uint8_t, 32> transferHash{};
    uint32_t transferSize = 0;
    uint32_t receivedBytes = 0;
    uint32_t lastProgressTime = 0;
    bool terminalHashValid = false;
    std::array<uint8_t, 32> terminalHash{};

    std::string pendingCommand;
    bool hasPendingCommand = false;
    bool oversizedPending = false;
    bool busyPending = false;
    std::optional<SignedStreamReply> disconnectedSnapshot;

    void synchronizeTransport();
    void clearTransfer(bool abortTransfer);
    std::optional<SignedStreamReply> processCommand(const std::string& command);
    SignedStreamReply errorReply(
        Error error, const StreamCommand* command = nullptr);
    SignedStreamReply signatureErrorReply(
        SignatureError error, uint32_t count, const std::string& hash);

    static const char* errorMessage(Error error);
    static Error errorFor(StreamCommandError error);
    static Error errorFor(SignatureError error);
    static SignedStreamReply transferReply(
        const std::string& status, uint32_t count, const std::string& hash);
};

#endif  // COMMON_SIGNEDSTREAMRECEIVER_HPP
