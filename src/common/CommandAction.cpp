#include "CommandAction.hpp"

#include "../tools/string.hpp"

void CommandAction::fire(const InterfaceConfig& /*interface*/) {
    std::string value = command->evaluate();
    if (value.length() != 0) {
        target.execute(value);
    }
}
