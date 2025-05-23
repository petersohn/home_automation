#include "PublishAction.hpp"
#include "common/MqttClient.hpp"
#include "tools/string.hpp"

PublishAction::PublishAction(
    std::ostream& debug, EspApi& esp, MqttClient& mqttClient,
    const std::string& topic, std::unique_ptr<operation::Operation>&& operation,
    bool retain, unsigned minimumSendInterval)
    : debug(debug)
    , esp(esp)
    , mqttClient(mqttClient)
    , topic(topic)
    , operation(std::move(operation))
    , retain(retain)
    , minimumSendInterval(minimumSendInterval)
    , lastSend(0) {}

void PublishAction::fire(const InterfaceConfig& /*interface*/) {
    auto now = esp.millis();
    if (lastSend != 0 && now - lastSend < minimumSendInterval) {
        debug << "Too soon, not sending." << std::endl;
        return;
    }

    std::string value = operation->evaluate();
    if (value.length() == 0) {
        debug << "No value for " + topic << std::endl;
        return;
    }
    mqttClient.publish(topic.c_str(), value.c_str(), retain);
    lastSend = now;
}
