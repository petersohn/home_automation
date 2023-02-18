#ifndef OPERATIONPARSER_HPP
#define OPERATIONPARSER_HPP

#include "Operation.hpp"
#include "../common/InterfaceConfig.hpp"
#include "../common/ArduinoJson.hpp"

#include <memory>
#include <unordered_set>

namespace operation {

struct MappingElement;

class Parser {
public:
    Parser(const std::vector<std::unique_ptr<InterfaceConfig>>& interfaces,
            InterfaceConfig* defaultInterface);

    std::unique_ptr<Operation> parse(const ArduinoJson::JsonObject& data,
            const char* fieldName, const char* templateFieldName);
    const std::unordered_set<InterfaceConfig*>& getUsedInterfaces() const& {
        return usedInterfaces;
    }
    std::unordered_set<InterfaceConfig*>&& getUsedInterfaces() && {
        return std::move(usedInterfaces);
    }

private:
    std::unique_ptr<Operation> doParse(const ArduinoJson::JsonVariant& data);
    std::vector<std::unique_ptr<Operation>> parseOperands(
            const ArduinoJson::JsonObject& object);
    std::vector<MappingElement> parseMappingElements(const ArduinoJson::JsonObject& object);

    std::vector<InterfaceConfig*> interfaces;
    InterfaceConfig* defaultInterface;
    std::unordered_set<InterfaceConfig*> usedInterfaces;
};

} // namespace operation

#endif // OPERATIONPARSER_HPP
