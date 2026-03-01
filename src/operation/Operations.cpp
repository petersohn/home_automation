#include "Operations.hpp"

#include "../tools/string.hpp"

namespace operation {

std::string Constant::evaluate() {
    return this->value;
}

std::string Value::evaluate() {
    if (!this->interface) {
        return {};
    }
    if (this->index > this->interface->storedValue.size()) {
        return {};
    }
    return this->interface->storedValue[this->index - 1];
}

std::string Template::evaluate() {
    if (!this->interface) {
        return this->template_;
    }
    return tools::substitute(this->template_, this->interface->storedValue);
}

std::string Conditional::evaluate() {
    bool value = false;
    auto valueStr = this->condition->evaluate();
    tools::getBoolValue(valueStr.c_str(), value, valueStr.size());
    return value ? this->then->evaluate() : this->else_->evaluate();
}

}  // namespace operation
