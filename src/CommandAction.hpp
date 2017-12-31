#ifndef COMMANDACTION_HPP
#define COMMANDACTION_HPP

#include "Action.hpp"
#include "Interface.hpp"

#include <Arduino.h>

class CommandAction : public Action {
public:
    CommandAction(Interface& target, const String& command)
            : target(target), command(command) {}

    void fire(const InterfaceConfig& interface);

private:
    Interface& target;
    String command;
};

#endif // COMMANDACTION_HPP


