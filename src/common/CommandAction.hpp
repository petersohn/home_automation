#ifndef COMMON_COMMANDACTION_HPP
#define COMMON_COMMANDACTION_HPP

#include "Action.hpp"
#include "Interface.hpp"

#include <string>

class CommandAction : public Action {
public:
    CommandAction(Interface& target, const std::string& command)
            : target(target), command(command) {}

    void fire(const InterfaceConfig& interface);

private:
    Interface& target;
    std::string command;
};

#endif // COMMON_COMMANDACTION_HPP


