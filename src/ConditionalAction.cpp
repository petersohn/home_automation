#include "ConditionalAction.hpp"

#include "string.hpp"

void ConditionalAction::fire(const std::vector<String>& values) {
    if (values[0] == value) {
        action->fire(values);
    } else {
        bool expected = false, actual = false;
        if (tools::getBoolValue(value, expected)
                && tools::getBoolValue(values[0], actual)
                && expected == actual) {
            action->fire(values);
        }
    }
}

