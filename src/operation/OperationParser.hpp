#ifndef OPERATIONPARSER_HPP
#define OPERATIONPARSER_HPP

#include "Operation.hpp"
#include "../common/InterfaceConfig.hpp"

#include "../ArduinoJson.hpp"

#include <memory>

namespace operation {

class Parser {
public:
    Parser(const std::vector<std::unique_ptr<InterfaceConfig>>& interfaces,
            const InterfaceConfig& interface);

    std::unique_ptr<Operation> parse(const JsonObject& data,
            const char* fieldName, const char* templateFieldName);

private:
    std::unique_ptr<Operation> doParse(const JsonVariant& data);
    std::vector<std::unique_ptr<Operation>> parseOperands(
            const JsonObject& object);

    std::vector<InterfaceConfig*> interfaces;
    const InterfaceConfig& interface;
};

} // namespace operation

#endif // OPERATIONPARSER_HPP
