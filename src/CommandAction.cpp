#include "CommandAction.hpp"

#include "string.hpp"

void CommandAction::fire(const std::vector<String>& values) {
    interface.execute(tools::substitute(command, values));
}
