#ifndef COMMANDACTION_HPP
#define COMMANDACTION_HPP

#include "Action.hpp"
#include "Interface.hpp"

#include <Arduino.h>

class CommandAction : public Action {
public:
    CommandAction(Interface& interface, const String& command)
            : interface(interface), command(command) {}

    void fire(const std::vector<String>& values);

private:
    Interface& interface;
    String command;
};

#endif // COMMANDACTION_HPP


