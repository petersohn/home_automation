#include "Operations.hpp"

#include "../tools/string.hpp"

namespace operation {

std::string Constant::evaluate() {
    return value;
}

std::string Value::evaluate() {
    if (!interface) {
        return {};
    }
    if (index > interface->storedValue.size()) {
        return {};
    }
    return interface->storedValue[index - 1];
}

std::string Template::evaluate() {
    if (!interface) {
        return template_;
    }
    return tools::substitute(template_, interface->storedValue);
}

std::string Conditional::evaluate() {
    bool value = false;
    tools::getBoolValue(condition->evaluate(), value);
    return value ? then->evaluate() : else_->evaluate();
}

} // namespace operation
