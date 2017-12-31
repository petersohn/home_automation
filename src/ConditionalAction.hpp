#ifndef CONDITIONALACTION_HPP
#define CONDITIONALACTION_HPP

#include "Action.hpp"

#include <Arduino.h>

#include <memory>

class ConditionalAction : public Action {
public:
    ConditionalAction(const String& value, std::unique_ptr<Action>&& action)
            : value(value), action(std::move(action)) {}

    void fire(const InterfaceConfig& interface);

private:
    String value;
    std::unique_ptr<Action> action;
};

#endif // CONDITIONALACTION_HPP

