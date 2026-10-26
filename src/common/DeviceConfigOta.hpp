#ifndef COMMON_DEVICE_CONFIG_OTA_HPP
#define COMMON_DEVICE_CONFIG_OTA_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "OtaFile.hpp"
#include "OtaTypes.hpp"
#include "SignedStreamReceiver.hpp"

class ConfigBootCoordinator;
enum class MqttReadiness;

class DeviceConfigOta final : private SignedStreamSink {
public:
    DeviceConfigOta(
        EspApi& esp, OtaFileSystem& fileSystem,
        ConfigBootCoordinator& bootCoordinator,
        std::unique_ptr<SignatureVerifier> verifier, std::string localMac,
        std::function<MqttTransportState()> transportState,
        std::function<void(const OtaResult&)> resultCallback);
    ~DeviceConfigOta() override = default;

    void execute(const std::string& command, bool retained);
    void update();
    void observeMqtt(MqttReadiness readiness, bool collided);

private:
    static constexpr const char* downloadPath = "/device_config.download.json";

    EspApi& esp;
    OtaFileSystem& fileSystem;
    ConfigBootCoordinator& bootCoordinator;
    std::string localMac;
    std::function<void(const OtaResult&)> resultCallback;
    SignedStreamReceiver receiver;
    std::unique_ptr<OtaFile> downloadFile;
    std::optional<OtaResult> deferredResult;
    std::optional<OtaResult> startupOutcome;
    MqttReadiness readiness;
    bool collisionPending = false;
    bool restartPending = false;
    bool storageFaulted = false;
    bool oversizedBeginRejected = false;
    bool preserveDownloadOnAbort = false;

    bool begin(uint32_t size) override;
    bool append(SignatureBytes data) override;
    std::string complete(const SignedStreamReply& verified) override;
    void abort() override;

    bool closeDownload();
    void processReply(const SignedStreamReply& reply);
    void publishResult(const OtaResult& result);
    bool processTrialSignal();
};

#endif  // COMMON_DEVICE_CONFIG_OTA_HPP
