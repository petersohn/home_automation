#include "PublishAction.hpp"
#include "client.hpp"
#include "debug.hpp"
#include "string.hpp"

void PublishAction::fire(const InterfaceConfig& interface) {
    debugln("Publishing to " + topic);
    std::string value = valueTemplate.length() == 0 ? interface.storedValue[0]
            : tools::substitute(valueTemplate, interface.storedValue);
    if (mqtt::client.publish(topic.c_str(), value.c_str(), retain)) {
        debugln("Success.");
    } else {
        debugln("Failure.");
    }
}
