#include "PublishAction.hpp"

#include "client.hpp"
#include "debug.hpp"
#include "tools/string.hpp"

void PublishAction::fire(const InterfaceConfig& /*interface*/) {
    std::string value = operation->evaluate();
    if (value.length() == 0) {
        debugln("No value for " + topic);
        return;
    }
    mqtt::publish(topic, value, retain);
}
