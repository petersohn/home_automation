#ifndef COMMON_COMMANDACTION_HPP
#define COMMON_COMMANDACTION_HPP

#include <string>

#include "../operation/Operation.hpp"
#include "Action.hpp"
#include "Interface.hpp"

class CommandAction : public Action {
public:
    CommandAction(
        Interface& target, std::unique_ptr<operation::Operation>&& command)
        : target(target), command(std::move(command)) {}

    void fire(const InterfaceConfig& interface);

private:
    Interface& target;
    std::unique_ptr<operation::Operation> command;
};

#endif  // COMMON_COMMANDACTION_HPP
