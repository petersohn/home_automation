#include "PublishAction.hpp"

#include "common/MqttClient.hpp"
#include "tools/fromString.hpp"

PublishAction::PublishAction(
    std::ostream& debug, EspApi& esp, MqttClient& mqttClient,
    const std::string& topic, std::unique_ptr<operation::Operation>&& operation,
    bool retain, unsigned minimumSendInterval, double sendDiff)
    : debug(debug)
    , esp(esp)
    , mqttClient(mqttClient)
    , topic(topic)
    , operation(std::move(operation))
    , retain(retain)
    , minimumSendInterval(minimumSendInterval)
    , sendDiff(sendDiff)
    , lastSend(0) {}

void PublishAction::reset() {
    this->lastSend = 0;
}

void PublishAction::fire(const InterfaceConfig& /*interface*/) {
    auto now = this->esp.millis();
    std::string value;
    std::optional<double> valueNum;
    if (this->sendDiff != 0.0) {
        value = this->operation->evaluate();
        if (value.empty()) {
            this->debug << "No value for " + this->topic << std::endl;
            return;
        }
        valueNum = tools::fromString<double>(value);
        if (!valueNum.has_value()) {
            this->debug << "Failed to parse numerical value: " << value
                        << std::endl;
        }
    }

    if (this->lastSend != 0 &&
        now - this->lastSend < this->minimumSendInterval &&
        (!valueNum.has_value() ||
         (this->lastSentValue.has_value() &&
          std::abs(*this->lastSentValue - *valueNum) < this->sendDiff))) {
        this->debug << "Too soon, not sending." << std::endl;
        return;
    }

    if (value.empty()) {
        value = this->operation->evaluate();
        if (value.empty()) {
            this->debug << "No value for " + this->topic << std::endl;
            return;
        }
    }

    this->mqttClient.publish(this->topic.c_str(), value.c_str(), this->retain);
    this->lastSend = now;
    this->lastSentValue = valueNum;
}
