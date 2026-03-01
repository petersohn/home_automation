#include "MqttInterface.hpp"

#include "common/MqttClient.hpp"

MqttInterface::~MqttInterface() {
    this->mqttClient.unsubscribe(this->topic.c_str());
}

void MqttInterface::start() {
    this->mqttClient.subscribe(
        this->topic.c_str(), [this](const MqttConnection::Message& message) {
        std::string payload(message.payload, message.payloadLength);
        this->onMessage(payload);
    });
}

void MqttInterface::execute(const std::string& /*command*/) {}

void MqttInterface::update(Actions action) {
    for (std::string& message : this->messages) {
        action.fire({std::move(message)});
    }
    this->messages.clear();
}

void MqttInterface::onMessage(const std::string& message) {
    this->messages.push_back(message);
}
