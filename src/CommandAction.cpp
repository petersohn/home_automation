#include "CommandAction.hpp"

#include "string.hpp"

void CommandAction::fire(const InterfaceConfig& interface) {
    target.execute(tools::substitute(command, interface.storedValue));
}
