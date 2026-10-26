#include "DeviceConfigOtaRuntime.hpp"

#include <Arduino.h>

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <utility>

#include "SignatureVerifierFactory.hpp"
#include "common/OtaLifecycleReadiness.hpp"
#include "common/OtaTypes.hpp"
#include "common/UploadStatus.hpp"

namespace {

bool isUsableServicePrefix(const std::string& prefix) {
    static constexpr std::size_t maxPrefixBytes = 482;
    if (prefix.empty() || prefix.size() > maxPrefixBytes) {
        return false;
    }
    for (char value : prefix) {
        if (value == '\0' || value == '#' || value == '+') {
            return false;
        }
    }
    return true;
}

bool isLifecycleStatus(const std::string& status) {
    return status == "STAGED" || status == "CONFIRMED" ||
           status == "ROLLED_BACK";
}

}  // namespace

DeviceConfigOtaRuntime::RestartEspApi::RestartEspApi(
    DeviceConfigOtaRuntime& runtime)
    : runtime(runtime) {}

void DeviceConfigOtaRuntime::RestartEspApi::pinMode(
    uint8_t pin, GpioMode mode) {
    this->runtime.esp.pinMode(pin, mode);
}

void DeviceConfigOtaRuntime::RestartEspApi::digitalWrite(
    uint8_t pin, uint8_t val) {
    this->runtime.esp.digitalWrite(pin, val);
}

int DeviceConfigOtaRuntime::RestartEspApi::digitalRead(uint8_t pin) {
    return this->runtime.esp.digitalRead(pin);
}

unsigned long DeviceConfigOtaRuntime::RestartEspApi::millis() {
    return this->runtime.esp.millis();
}

unsigned long DeviceConfigOtaRuntime::RestartEspApi::micros() {
    return this->runtime.esp.micros();
}

void DeviceConfigOtaRuntime::RestartEspApi::delay(unsigned long ms) {
    this->runtime.esp.delay(ms);
}

void DeviceConfigOtaRuntime::RestartEspApi::restart(bool hard) {
    Serial.printf("[OTA] restart at %lu ms\n", this->runtime.esp.millis());
    this->runtime.mqttClient.disconnect();
    this->runtime.esp.restart(hard);
}

uint32_t DeviceConfigOtaRuntime::RestartEspApi::getFreeHeap() {
    return this->runtime.esp.getFreeHeap();
}

void DeviceConfigOtaRuntime::RestartEspApi::doDisableInterrupt() {
    this->runtime.esp.doDisableInterrupt();
}

void DeviceConfigOtaRuntime::RestartEspApi::doEnableInterrupt() {
    this->runtime.esp.doEnableInterrupt();
}

void DeviceConfigOtaRuntime::RestartEspApi::setRush(
    unsigned long microseconds) {
    this->runtime.esp.setRush(microseconds);
}

DeviceConfigOtaRuntime::DeviceConfigOtaRuntime(
    std::ostream& debug, EspApi& esp, MqttClient& mqttClient, Wifi& wifi)
    : debug(debug)
    , esp(esp)
    , mqttClient(mqttClient)
    , wifi(wifi)
    , restartEsp(*this)
    , journal(this->fileSystem)
    , bootCoordinator(this->fileSystem, this->journal) {}

const char* DeviceConfigOtaRuntime::selectConfigPath() {
    if (!this->selected) {
        this->selected = true;
        if (!this->fileSystem.mount()) {
            this->debug << "Could not mount SPIFFS; configuration OTA disabled."
                        << std::endl;
            this->selection.reason = "filesystem-mount-failed";
            return configSlotPath(ConfigSlot::primary);
        }

        this->selection = this->bootCoordinator.selectForBoot();
        if (this->selection.reason == "trial") {
            Serial.begin(115200);
            Serial.println("Configuration boot selection: trial");
        }
        if (!this->selection.reason.empty()) {
            this->debug << "Configuration boot selection: "
                        << this->selection.reason << std::endl;
        }
        const OtaStatusSnapshot snapshot = this->bootCoordinator.snapshot();
        if (this->selection.reason.size() != 0 &&
            snapshot.lastOutcome.has_value() &&
            snapshot.lastOutcome->status == "ROLLED_BACK" &&
            snapshot.lastOutcome->reason == this->selection.reason) {
            this->restartPending = true;
        }
    }
    return configSlotPath(this->selection.slot);
}

void DeviceConfigOtaRuntime::begin(const DeviceConfigCommon& common) {
    if (this->begun) {
        return;
    }
    this->begun = true;
    if (!this->selected) {
        this->selectConfigPath();
    }
    if (!this->fileSystem.mounted()) {
        return;
    }

    const OtaStatusSnapshot initial = this->bootCoordinator.snapshot();
    this->configurationPolicyValid =
        common.configVersionValid && common.otaEnabled &&
        common.configVersion == initial.configVersion &&
        isUsableServicePrefix(common.servicePrefix);
    if (isUsableServicePrefix(common.servicePrefix)) {
        this->resultTopic = common.servicePrefix;
        this->resultTopic += "/update/device-config/result";
        this->commandTopic = common.servicePrefix;
        this->commandTopic += "/update/device-config";
    }

    std::unique_ptr<SignatureVerifier> verifier =
        loadSignatureVerifier(this->debug, "/ota_certificate.pem");
    this->certificateValid =
        verifier != nullptr && verifier->begin() == SignatureError::none;
    if (verifier != nullptr) {
        verifier->abort();
    }

    if (this->selection.trial &&
        (!this->configurationPolicyValid || !this->certificateValid)) {
        const char* reason = !this->configurationPolicyValid
                                 ? "candidate-config-invalid"
                                 : "certificate-invalid";
        (void)this->bootCoordinator.rollback(reason);
        this->restartPending = true;
    }

    if (this->journal.state().has_value()) {
        this->mqttClient.setOtaStatusProvider([this]() {
            OtaStatusSnapshot snapshot = this->bootCoordinator.snapshot();
            if (!this->configurationPolicyValid || !this->certificateValid) {
                snapshot.updatesAllowed = false;
            }
            return snapshot;
        });
    }

    this->ota = std::make_unique<DeviceConfigOta>(
        this->restartEsp, this->fileSystem, this->bootCoordinator,
        std::move(verifier), this->wifi.getMac(), [this]() {
        return this->mqttClient.getTransportState();
    }, [this](const OtaResult& result) { this->reportResult(result); });

    const OtaStatusSnapshot current = this->bootCoordinator.snapshot();
    const bool monitorLifecycle =
        this->selection.trial || current.phase != UploadPhase::idle ||
        !current.minimumAcceptedConfigVersion.has_value();
    const bool serviceEnabled = isUsableServicePrefix(common.servicePrefix) &&
                                (common.otaEnabled || monitorLifecycle);
    if (serviceEnabled) {
        this->serviceSubscribed = true;
        this->mqttClient.subscribePriority(
            this->commandTopic.c_str(),
            [this](const MqttConnection::Message& message) {
            if (message.payload == nullptr && message.payloadLength != 0) {
                return;
            }
            const std::string command =
                message.payload == nullptr
                    ? std::string()
                    : std::string(message.payload, message.payloadLength);
            this->ota->execute(command, message.retain);
        }, 1);
    }
}

void DeviceConfigOtaRuntime::update() {
    if (this->ota != nullptr) {
        const MqttReadiness readiness = this->mqttClient.getReadiness();
        bool collided = this->mqttClient.takeCollision();
        if (readiness == MqttReadiness::collision) {
            collided = true;
        }
        const OtaLifecycleReadiness lifecycleReadiness{
            readiness == MqttReadiness::ready,
            this->mqttClient.canPublish(),
            this->certificateValid,
            this->configurationPolicyValid,
            this->serviceSubscribed,
            this->mqttClient.subscriptionReady(this->commandTopic.c_str())};
        const bool ready = isOtaLifecycleReady(lifecycleReadiness);
        this->ota->observeMqtt(
            ready ? MqttReadiness::ready : MqttReadiness::checking, collided);
        this->ota->update();
    }

    this->publishPendingResult();
    if (this->restartPending) {
        this->restartPending = false;
        this->restartEsp.restart(false);
    }
}

void DeviceConfigOtaRuntime::reportResult(const OtaResult& result) {
    const std::string payload = encodeOtaResult(result);
    if (isLifecycleStatus(result.status)) {
        Serial.printf(
            "[OTA] outcome at %lu ms status=%s encoded-length=%lu\n",
            this->esp.millis(), result.status.c_str(),
            static_cast<unsigned long>(payload.size()));
    }
    if (payload.empty()) {
        return;
    }
    this->resultPublisher.report(result);
}

void DeviceConfigOtaRuntime::publishPendingResult() {
    const bool lifecyclePending = this->resultPublisher.hasPendingLifecycle();
    if (!lifecyclePending) {
        this->loggedLifecycleEmptyTopicGate = false;
        this->loggedLifecycleReadinessGate = false;
        this->loggedLifecycleCanPublishGate = false;
    }

    if (this->resultTopic.empty()) {
        if (lifecyclePending && !this->loggedLifecycleEmptyTopicGate) {
            Serial.println(
                "[OTA] pending lifecycle publish blocked: empty topic");
            this->loggedLifecycleEmptyTopicGate = true;
        }
        return;
    }

    const MqttReadiness readiness = this->mqttClient.getReadiness();
    if (readiness != MqttReadiness::ready) {
        if (lifecyclePending && !this->loggedLifecycleReadinessGate) {
            Serial.printf(
                "[OTA] pending lifecycle publish blocked: readiness-enum=%u\n",
                static_cast<unsigned int>(readiness));
            this->loggedLifecycleReadinessGate = true;
        }
        return;
    }

    const bool canPublish = this->mqttClient.canPublish();
    if (!canPublish) {
        if (lifecyclePending && !this->loggedLifecycleCanPublishGate) {
            Serial.println(
                "[OTA] pending lifecycle publish blocked: canPublish=false");
            this->loggedLifecycleCanPublishGate = true;
        }
        return;
    }

    this->resultPublisher.flush([this](const OtaResult& result) {
        const std::string payload = encodeOtaResult(result);
        if (payload.empty()) {
            return false;
        }
        const bool submitted = this->mqttClient.tryPublish(
            this->resultTopic.c_str(), payload.c_str(), false);
        if (isLifecycleStatus(result.status)) {
            Serial.printf(
                "[OTA] lifecycle submit status=%s topic=%s returned=%s\n",
                result.status.c_str(), this->resultTopic.c_str(),
                submitted ? "true" : "false");
        }
        return submitted;
    });
}
