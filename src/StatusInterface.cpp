#include "StatusInterface.hpp"

#include "common/MqttClient.hpp"
#include "tools/string.hpp"

void StatusInterface::start() {
    this->value = -1;
}

void StatusInterface::execute(const std::string& /*command*/) {}

void StatusInterface::update(Actions action) {
    int newValue = this->mqttClient.isConnected() ? 1 : 0;
    if (newValue != this->value) {
        this->value = newValue;
        action.fire({tools::intToString(this->value)});
    }
}
