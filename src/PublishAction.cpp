#include "PublishAction.hpp"

#include "client.hpp"
#include "debug.hpp"
#include "tools/string.hpp"

void PublishAction::fire(const InterfaceConfig& /*interface*/) {
    debugln("Publishing to " + topic);
    std::string value = operation->evaluate();
    if (value.length() == 0) {
        return;
    }
    if (mqtt::client.publish(topic.c_str(), value.c_str(), retain)) {
        debugln("Success.");
    } else {
        debugln("Failure.");
    }
}
