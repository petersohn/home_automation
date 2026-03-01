#include "Actions.hpp"

void Actions::fire(const std::vector<std::string>& values) {
    interface.storedValue = values;
    if (values.empty()) {
        return;
    }
    for (const auto& action : interface.actions) {
        action->fire(interface);
    }
}

void Actions::reset() {
    for (const auto& action : interface.actions) {
        action->reset();
    }
}
