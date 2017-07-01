#include "PublishAction.hpp"
#include "client.hpp"
#include "debug.hpp"
#include "string.hpp"

void PublishAction::fire(const std::vector<String>& values) {
    DEBUGLN("Publishing to " + topic);
    String value = valueTemplate.length() == 0 ? values[0]
            : tools::substitute(valueTemplate, values);
    if (mqttClient.publish(topic.c_str(), value.c_str(), retain)) {
        DEBUGLN("Success.");
    } else {
        DEBUGLN("Failure.");
    }
}
