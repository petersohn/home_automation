#include "MqttInterface.hpp"
#include "common/MqttClient.hpp"

MqttInterface::~MqttInterface() {
    mqttClient.unsubscribe(topic.c_str());
}

void MqttInterface::start() {
    mqttClient.subscribe(
        topic.c_str(), [this](const MqttConnection::Message& message) {
            std::string payload(
                message.payload, message.payloadLength);  // FIXME
            onMessage(payload);
        });
}

void MqttInterface::execute(const std::string& /*command*/) {}

void MqttInterface::update(Actions action) {
    for (std::string& message : messages) {
        action.fire({std::move(message)});
    }
    messages.clear();
}

void MqttInterface::onMessage(const std::string& message) {
    messages.push_back(message);
}
