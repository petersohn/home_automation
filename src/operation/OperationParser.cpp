#include <functional>
#include <memory>

#include "../common/InterfaceConfig.hpp"
#include "OperationParser.hpp"
#include "Operations.hpp"
#include "Translator.hpp"

using namespace ArduinoJson;

namespace operation {

namespace {

std::unique_ptr<Operation> getEmptyOperation() {
    return std::make_unique<Constant>("");
}

}  // unnamed namespace

Parser::Parser(
    const std::vector<std::unique_ptr<InterfaceConfig>>& interfaces,
    InterfaceConfig* defaultInterface)
    : interfaces(interfaces.size(), nullptr)
    , defaultInterface(defaultInterface) {
    std::transform(
        interfaces.begin(), interfaces.end(), this->interfaces.begin(),
        [](const std::unique_ptr<InterfaceConfig>& interface) {
            return interface.get();
        });
}

std::unique_ptr<Operation> Parser::parse(
    const JsonObject& data, const char* fieldName,
    const char* templateFieldName) {
    std::string template_ =
        templateFieldName ? data.get<std::string>(templateFieldName) : "";
    auto operation = template_.length() != 0 ? std::make_unique<Template>(
                                                   defaultInterface, template_)
                                             : doParse(data[fieldName]);
    std::string value = data["value"];
    if (!value.empty()) {
        std::vector<std::unique_ptr<Operation>> operands;
        operands.push_back(std::make_unique<Value>(defaultInterface, 1));
        operands.push_back(std::make_unique<Constant>(value));
        return std::make_unique<Conditional>(
            std::make_unique<
                Comparison<std::equal_to<std::string>, translator::Str>>(
                std::move(operands)),
            std::move(operation), getEmptyOperation());
    }
    return operation;
}

std::unique_ptr<Operation> Parser::doParse(const JsonVariant& data) {
    const auto& object = data.as<JsonObject>();
    if (object == JsonObject::invalid()) {
        return std::make_unique<Constant>(data.as<std::string>());
    }

    auto type = object.get<std::string>("type");
    if (type == "value") {
        auto name = object.get<std::string>("interface");
        auto template_ = object.get<std::string>("template");
        auto interface = name.empty() ? this->defaultInterface
                                      : findInterface(interfaces, name);
        if (!interface) {
            return getEmptyOperation();
        }
        usedInterfaces.insert(interface);
        if (template_.length() != 0) {
            return std::make_unique<Template>(interface, template_);
        } else {
            auto index = object.get<std::size_t>("index");
            if (index == 0) {
                index = 1;
            }
            return std::make_unique<Value>(interface, index);
        }
    } else if (type == "+") {
        return std::make_unique<
            FoldingOperation<std::plus<float>, translator::Float>>(
            parseOperands(object));
    } else if (type == "-") {
        return std::make_unique<
            FoldingOperation<std::minus<float>, translator::Float>>(
            parseOperands(object));
    } else if (type == "*") {
        return std::make_unique<
            FoldingOperation<std::multiplies<float>, translator::Float>>(
            parseOperands(object));
    } else if (type == "/") {
        return std::make_unique<
            FoldingOperation<std::divides<float>, translator::Float>>(
            parseOperands(object));
    } else if (type == "s+") {
        return std::make_unique<
            FoldingOperation<std::plus<std::string>, translator::Str>>(
            parseOperands(object));
    } else if (type == "=") {
        return std::make_unique<
            Comparison<std::equal_to<float>, translator::Float>>(
            parseOperands(object));
    } else if (type == "s=") {
        return std::make_unique<
            Comparison<std::equal_to<std::string>, translator::Str>>(
            parseOperands(object));
    } else if (type == "!=") {
        return std::make_unique<
            Comparison<std::not_equal_to<float>, translator::Float>>(
            parseOperands(object));
    } else if (type == "s!=") {
        return std::make_unique<
            Comparison<std::not_equal_to<std::string>, translator::Str>>(
            parseOperands(object));
    } else if (type == "<") {
        return std::make_unique<
            Comparison<std::less<float>, translator::Float>>(
            parseOperands(object));
    } else if (type == "s<") {
        return std::make_unique<
            Comparison<std::less<std::string>, translator::Str>>(
            parseOperands(object));
    } else if (type == ">") {
        return std::make_unique<
            Comparison<std::greater<float>, translator::Float>>(
            parseOperands(object));
    } else if (type == "s>") {
        return std::make_unique<
            Comparison<std::greater<std::string>, translator::Str>>(
            parseOperands(object));
    } else if (type == "<=") {
        return std::make_unique<
            Comparison<std::less_equal<float>, translator::Float>>(
            parseOperands(object));
    } else if (type == "s<=") {
        return std::make_unique<
            Comparison<std::less_equal<std::string>, translator::Str>>(
            parseOperands(object));
    } else if (type == ">=") {
        return std::make_unique<
            Comparison<std::greater_equal<float>, translator::Float>>(
            parseOperands(object));
    } else if (type == "s>=") {
        return std::make_unique<
            Comparison<std::greater_equal<std::string>, translator::Str>>(
            parseOperands(object));
    } else if (type == "&") {
        return std::make_unique<
            FoldingOperation<std::logical_and<bool>, translator::Bool>>(
            parseOperands(object));
    } else if (type == "|") {
        return std::make_unique<
            FoldingOperation<std::logical_or<bool>, translator::Bool>>(
            parseOperands(object));
    } else if (type == "!") {
        return std::make_unique<
            UnaryOperation<std::logical_not<bool>, translator::Bool>>(
            doParse(object["op"]));
    } else if (type == "if") {
        return std::make_unique<Conditional>(
            doParse(object["cond"]), doParse(object["then"]),
            doParse(object["else"]));
    } else if (type == "map") {
        return std::make_unique<Mapping<translator::Float>>(
            parseMappingElements(object), doParse(object["value"]));
    } else if (type == "smap") {
        return std::make_unique<Mapping<translator::Str>>(
            parseMappingElements(object), doParse(object["value"]));
    } else {
        return getEmptyOperation();
    }
}

std::vector<std::unique_ptr<Operation>> Parser::parseOperands(
    const JsonObject& object) {
    std::vector<std::unique_ptr<Operation>> result;
    for (const JsonVariant& element : object.get<JsonArray>("ops")) {
        result.push_back(doParse(element));
    }
    return result;
}

std::vector<MappingElement> Parser::parseMappingElements(
    const JsonObject& object) {
    std::vector<MappingElement> elements;
    for (const JsonVariant& elementData : object.get<JsonArray>("ops")) {
        const auto& elementObject = elementData.as<JsonObject>();
        if (elementObject == JsonObject::invalid()) {
            continue;
        }

        elements.push_back(MappingElement{
            doParse(elementObject["min"]), doParse(elementObject["max"]),
            doParse(elementObject["value"])});
    }
    return elements;
}

}  // namespace operation
