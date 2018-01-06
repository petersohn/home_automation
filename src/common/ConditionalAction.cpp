#include "ConditionalAction.hpp"

#include "../tools/string.hpp"

void ConditionalAction::fire(const InterfaceConfig& interface) {
    const std::string& toCompare = interface.storedValue[0];
    if (toCompare == value) {
        action->fire(interface);
    } else {
        bool expected = false, actual = false;
        if (tools::getBoolValue(value, expected)
                && tools::getBoolValue(toCompare, actual)
                && expected == actual) {
            action->fire(interface);
        }
    }
}

