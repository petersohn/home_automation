#ifndef COMMON_SIGNATUREVERIFYINTERFACE_HPP
#define COMMON_SIGNATUREVERIFYINTERFACE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Interface.hpp"
#include "MqttTransportState.hpp"
#include "SignatureVerifier.hpp"
#include "StreamCommand.hpp"

class EspApi;

class SignatureVerifyInterface : public Interface {
public:
    SignatureVerifyInterface(
        EspApi& esp, std::unique_ptr<SignatureVerifier> verifier,
        std::function<MqttTransportState()> transportState);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

    ~SignatureVerifyInterface() override = default;

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
    std::function<MqttTransportState()> transportState;

    bool transportInitialized = false;
    MqttTransportState observedTransport{false, 0};
    bool initialEventPending = true;
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
    std::vector<std::string> disconnectedSnapshot;

    void synchronizeTransport();
    void clearTransfer(bool abortVerifier);
    std::vector<std::string> processCommand(const std::string& command);
    std::vector<std::string> errorReply(
        Error error, const StreamCommand* command = nullptr);
    std::vector<std::string> signatureErrorReply(
        SignatureError error, uint32_t count, const std::string& hash);

    static const char* errorMessage(Error error);
    static Error errorFor(StreamCommandError error);
    static Error errorFor(SignatureError error);
    static std::vector<std::string> transferReply(
        const std::string& status, uint32_t count, const std::string& hash);
};

#endif  // COMMON_SIGNATUREVERIFYINTERFACE_HPP
