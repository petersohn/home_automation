#include "MqttInterface.hpp"

#include "client.hpp"

MqttInterface::~MqttInterface() {
    mqtt::unsubscribe(topic);
}

void MqttInterface::start() {
    mqtt::subscribe(topic,
            [this](const String& message) {
                onMessage(message);
            });
}

void MqttInterface::execute(const String& /*command*/) {
}

void MqttInterface::update(Actions action) {
    for (const String& message : messages) {
        action.fire({message});
    }
    messages.clear();
}

void MqttInterface::onMessage(const String& message) {
    messages.push_back(message);
}
