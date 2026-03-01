#include "Actions.hpp"

void Actions::fire(const std::vector<std::string>& values) {
    this->interface.storedValue = values;
    if (values.empty()) {
        return;
    }
    for (const auto& action : this->interface.actions) {
        action->fire(this->interface);
    }
}

void Actions::reset() {
    for (const auto& action : this->interface.actions) {
        action->reset();
    }
}
