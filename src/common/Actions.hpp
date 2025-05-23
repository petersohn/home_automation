#ifndef COMMON_ACTIONS_HPP
#define COMMON_ACTIONS_HPP

#include "Action.hpp"

class Actions {
public:
    Actions(InterfaceConfig& interface) : interface(interface) {}

    void fire(const std::vector<std::string>& values) {
        interface.storedValue = values;
        if (values.empty()) {
            return;
        }
        for (const auto& action : interface.actions) {
            action->fire(interface);
        }
    }

private:
    InterfaceConfig& interface;
};

#endif  // COMMON_ACTIONS_HPP
