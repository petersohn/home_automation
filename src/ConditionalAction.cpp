#include "ConditionalAction.hpp"

#include "string.hpp"

void ConditionalAction::fire(const std::vector<String>& values) {
    if (values[0] == value) {
        action->fire(values);
    } else {
        bool expected = false, actual = false;
        if (tools::getBoolValue(value, expected, false)
                && tools::getBoolValue(values[0], actual, false)
                && expected == actual) {
            action->fire(values);
        }
    }
}

