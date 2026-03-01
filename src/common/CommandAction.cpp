#include "CommandAction.hpp"

#include "../tools/string.hpp"

void CommandAction::fire(const InterfaceConfig& /*interface*/) {
    std::string value = this->command->evaluate();
    if (value.length() != 0) {
        this->target.execute(value);
    }
}

void CommandAction::reset() {}
