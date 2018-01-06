#ifndef ACTION_HPP
#define ACTION_HPP

#include "config.hpp"

#include <memory>
#include <string>
#include <vector>

class Action {
public:
    virtual void fire(const InterfaceConfig& interface) = 0;
    virtual ~Action() {}
};

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

#endif // ACTION_HPP
