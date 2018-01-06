#include "MqttInterface.hpp"

#include "client.hpp"

MqttInterface::~MqttInterface() {
    mqtt::unsubscribe(topic);
}

void MqttInterface::start() {
    mqtt::subscribe(topic,
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
