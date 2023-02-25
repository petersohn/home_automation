#include "MqttInterface.hpp"

#include "common/MqttClient.hpp"

MqttInterface::~MqttInterface() {
    mqttClient.unsubscribe(topic);
}

void MqttInterface::start() {
    mqttClient.subscribe(topic,
            [this](const std::string& message) {
                onMessage(message);
            });
}

void MqttInterface::execute(const std::string& /*command*/) {
}

void MqttInterface::update(Actions action) {
    for (const std::string& message : messages) {
        action.fire({message});
    }
    messages.clear();
}

void MqttInterface::onMessage(const std::string& message) {
    messages.push_back(message);
}
