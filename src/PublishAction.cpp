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

void PublishAction::fire(const InterfaceConfig& /*interface*/) {
    auto now = esp.millis();
    std::string value;
    std::optional<double> valueNum;
    if (sendDiff != 0.0) {
        value = operation->evaluate();
        if (value.empty()) {
            debug << "No value for " + topic << std::endl;
            return;
        }
        valueNum = tools::fromString<double>(value);
        if (!valueNum.has_value()) {
            debug << "Failed to parse numerical value: " << value << std::endl;
        }
    }

    if (lastSend != 0 && now - lastSend < minimumSendInterval &&
        (!valueNum.has_value() ||
         (lastSentValue.has_value() &&
          std::abs(*lastSentValue - *valueNum) < sendDiff))) {
        debug << "Too soon, not sending." << std::endl;
        return;
    }

    if (value.empty()) {
        value = operation->evaluate();
        if (value.empty()) {
            debug << "No value for " + topic << std::endl;
            return;
        }
    }

    mqttClient.publish(topic.c_str(), value.c_str(), retain);
    lastSend = now;
    lastSentValue = valueNum;
}
