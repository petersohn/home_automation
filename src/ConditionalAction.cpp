#include "ConditionalAction.hpp"

void ConditionalAction::fire(const std::vector<String>& values) {
    if (values[0] == value) {
        action->fire(values);
    }
}

