#ifndef DEVICE_CONFIG_OTA_RUNTIME_HPP
#define DEVICE_CONFIG_OTA_RUNTIME_HPP

#include <memory>
#include <optional>
#include <ostream>
#include <string>

#include "SpiffsOtaFileSystem.hpp"
#include "common/ConfigBootCoordinator.hpp"
#include "common/DeviceConfigOta.hpp"
#include "common/EspApi.hpp"
#include "common/MqttClient.hpp"
#include "common/OtaResultPublisher.hpp"
#include "common/Wifi.hpp"

class DeviceConfigOtaRuntime {
public:
    DeviceConfigOtaRuntime(
        std::ostream& debug, EspApi& esp, MqttClient& mqttClient, Wifi& wifi);

    const char* selectConfigPath();
    void begin(const DeviceConfigCommon& common);
    void update();

private:
    class RestartEspApi final : public EspApi {
    public:
        explicit RestartEspApi(DeviceConfigOtaRuntime& runtime);

        void pinMode(uint8_t pin, GpioMode mode) override;
        void digitalWrite(uint8_t pin, uint8_t val) override;
        int digitalRead(uint8_t pin) override;
        unsigned long millis() override;
        unsigned long micros() override;
        void delay(unsigned long ms) override;
        void restart(bool hard) override;
        uint32_t getFreeHeap() override;
        void doDisableInterrupt() override;
        void doEnableInterrupt() override;
        void setRush(unsigned long microseconds) override;

    private:
        DeviceConfigOtaRuntime& runtime;
    };

    std::ostream& debug;
    EspApi& esp;
    MqttClient& mqttClient;
    Wifi& wifi;
    RestartEspApi restartEsp;
    SpiffsOtaFileSystem fileSystem;
    UploadJournal journal;
    ConfigBootCoordinator bootCoordinator;
    ConfigBootSelection selection;
    std::unique_ptr<DeviceConfigOta> ota;
    OtaResultPublisher resultPublisher;
    std::string commandTopic;
    std::string resultTopic;
    bool selected = false;
    bool begun = false;
    bool serviceSubscribed = false;
    bool configurationPolicyValid = false;
    bool certificateValid = false;
    bool restartPending = false;
    bool loggedLifecycleEmptyTopicGate = false;
    bool loggedLifecycleReadinessGate = false;
    bool loggedLifecycleCanPublishGate = false;

    void reportResult(const OtaResult& result);
    void publishPendingResult();
};

#endif  // DEVICE_CONFIG_OTA_RUNTIME_HPP
