#include "StatusInterface.hpp"
#include "client.hpp"
#include "tools/string.hpp"

void StatusInterface::start() {
    value = -1;
}

void StatusInterface::execute(const std::string& /*command*/) {
}

void StatusInterface::update(Actions action) {
    int newValue = mqtt::isConnected() ? 1 : 0;
    if (newValue != value) {
        value = newValue;
        action.fire({tools::intToString(value)});
    }
}