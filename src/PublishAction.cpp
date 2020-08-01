#include "PublishAction.hpp"

#include "client.hpp"
#include "debug.hpp"
#include "tools/string.hpp"

void PublishAction::fire(const InterfaceConfig& /*interface*/) {
    auto now = millis();
    if (lastSend != 0 && now - lastSend < minimumSendInterval) {
        debugln("Too soon, not sending.");
        return;
    }

    std::string value = operation->evaluate();
    if (value.length() == 0) {
        debugln("No value for " + topic);
        return;
    }
    mqtt::publish(topic, value, retain);
    lastSend = now;
}
