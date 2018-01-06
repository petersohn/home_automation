#ifndef CONDITIONALACTION_HPP
#define CONDITIONALACTION_HPP

#include "Action.hpp"

#include <memory>

class ConditionalAction : public Action {
public:
    ConditionalAction(const std::string& value,
            std::unique_ptr<Action>&& action)
            : value(value), action(std::move(action)) {}

    void fire(const InterfaceConfig& interface);

private:
    std::string value;
    std::unique_ptr<Action> action;
};

#endif // CONDITIONALACTION_HPP

