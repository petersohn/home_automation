#include "ConditionalAction.hpp"

#include "string.hpp"

void ConditionalAction::fire(const InterfaceConfig& interface) {
    const String& toCompare = interface.storedValue[0];
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

