#include "PublishAction.hpp"
#include "client.hpp"
#include "debug.hpp"

void PublishAction::fire(const String& value) {
    DEBUGLN("Publishing to " + topic);
    if (mqttClient.publish(topic.c_str(), value.c_str(), retain)) {
        DEBUGLN("Success.");
    } else {
        DEBUGLN("Failure.");
    }
}
