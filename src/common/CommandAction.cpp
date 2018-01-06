#include "CommandAction.hpp"

#include "../tools/string.hpp"

void CommandAction::fire(const InterfaceConfig& interface) {
    target.execute(tools::substitute(command, interface.storedValue));
}
