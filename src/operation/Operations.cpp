#include "../tools/string.hpp"
#include "Operations.hpp"

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
    auto valueStr = condition->evaluate();
    tools::getBoolValue(valueStr.c_str(), value, valueStr.size());
    return value ? then->evaluate() : else_->evaluate();
}

}  // namespace operation
