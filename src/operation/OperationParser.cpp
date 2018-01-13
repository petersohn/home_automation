#include "OperationParser.hpp"

#include "Operations.hpp"
#include "Translator.hpp"
#include "../common/InterfaceConfig.hpp"

#include <functional>

namespace operation {

namespace {

std::unique_ptr<Operation> getEmptyOperation() {
    return std::unique_ptr<Operation>(new Constant(""));
}

} // unnamed namespace

Parser::Parser(const std::vector<std::unique_ptr<InterfaceConfig>>& interfaces,
        const InterfaceConfig& interface)
        : interfaces(interfaces.size(), nullptr),
          interface(interface) {
    std::transform(interfaces.begin(), interfaces.end(),
            this->interfaces.begin(),
            [](const std::unique_ptr<InterfaceConfig>& interface) {
                return interface.get();
            });
}

std::unique_ptr<Operation> Parser::parse(const JsonObject& data,
        const char* fieldName, const char* templateFieldName) {
   std::string template_ = templateFieldName
           ? data.get<std::string>(templateFieldName) : "";
   auto operation = template_.length() != 0
           ? std::unique_ptr<Operation>(new Template(&interface, template_))
           : doParse(data[fieldName]);
   std::string value = data["value"];
   if (!value.empty()) {
       std::vector<std::unique_ptr<Operation>> operands;
       operands.push_back(std::unique_ptr<Operation>(
               new Value(&interface, 1)));
       operands.push_back(std::unique_ptr<Operation>(
               new Constant(value)));
       return std::unique_ptr<Operation>(new Conditional(
               std::unique_ptr<Operation>(new Comparison<
               std::equal_to<std::string>, translator::Str>(
                       std::move(operands))),
               std::move(operation), getEmptyOperation()));
   }
   return operation;
}

std::unique_ptr<Operation> Parser::doParse(const JsonVariant& data) {
   const auto& object = data.as<JsonObject>();
   if (object == JsonObject::invalid()) {
       return std::unique_ptr<Operation>(new Constant(
               data.as<std::string>()));
   }

   auto type = object.get<std::string>("type");
   if (type == "value") {
       auto name = object.get<std::string>("interface");
       auto template_ = object.get<std::string>("template");
       auto interface = name.empty()
               ? &this->interface : findInterface(interfaces, name);
       if (!interface) {
           return getEmptyOperation();
       }
       if (template_.length() != 0) {
           return std::unique_ptr<Operation>(
                   new Template(interface, template_));
       } else {
           auto index = object.get<std::size_t>("index");
           if (index == 0) {
               index = 1;
           }
           return std::unique_ptr<Operation>(new Value(interface, index));
       }
   } else if (type == "+") {
       return std::unique_ptr<Operation>(new FoldingOperation<
               std::plus<float>, translator::Float>(
               parseOperands(object)));
   } else if (type == "-") {
       return std::unique_ptr<Operation>(new FoldingOperation<
               std::minus<float>, translator::Float>(
               parseOperands(object)));
   } else if (type == "*") {
       return std::unique_ptr<Operation>(new FoldingOperation<
               std::multiplies<float>, translator::Float>(
               parseOperands(object)));
   } else if (type == "/") {
       return std::unique_ptr<Operation>(new FoldingOperation<
               std::divides<float>, translator::Float>(
               parseOperands(object)));
   } else if (type == "s+") {
       return std::unique_ptr<Operation>(new FoldingOperation<
               std::plus<std::string>, translator::Str>(
               parseOperands(object)));
   } else if (type == "=") {
       return std::unique_ptr<Operation>(new Comparison<
               std::equal_to<float>, translator::Float>(
               parseOperands(object)));
   } else if (type == "s=") {
       return std::unique_ptr<Operation>(new Comparison<
               std::equal_to<std::string>, translator::Str>(
               parseOperands(object)));
   } else if (type == "!=") {
       return std::unique_ptr<Operation>(new Comparison<
               std::not_equal_to<float>, translator::Float>(
               parseOperands(object)));
   } else if (type == "s!=") {
       return std::unique_ptr<Operation>(new Comparison<
               std::not_equal_to<std::string>, translator::Str>(
               parseOperands(object)));
   } else if (type == "<") {
       return std::unique_ptr<Operation>(new Comparison<
               std::less<float>, translator::Float>(
               parseOperands(object)));
   } else if (type == "s<") {
       return std::unique_ptr<Operation>(new Comparison<
               std::less<std::string>, translator::Str>(
               parseOperands(object)));
   } else if (type == ">") {
       return std::unique_ptr<Operation>(new Comparison<
               std::greater<float>, translator::Float>(
               parseOperands(object)));
   } else if (type == "s>") {
       return std::unique_ptr<Operation>(new Comparison<
               std::greater<std::string>, translator::Str>(
               parseOperands(object)));
   } else if (type == "<=") {
       return std::unique_ptr<Operation>(new Comparison<
               std::less_equal<float>, translator::Float>(
               parseOperands(object)));
   } else if (type == "s<=") {
       return std::unique_ptr<Operation>(new Comparison<
               std::less_equal<std::string>, translator::Str>(
               parseOperands(object)));
   } else if (type == ">=") {
       return std::unique_ptr<Operation>(new Comparison<
               std::greater_equal<float>, translator::Float>(
               parseOperands(object)));
   } else if (type == "s>=") {
       return std::unique_ptr<Operation>(new Comparison<
               std::greater_equal<std::string>, translator::Str>(
               parseOperands(object)));
   } else if (type == "&") {
       return std::unique_ptr<Operation>(new FoldingOperation<
               std::logical_and<bool>, translator::Bool>(
               parseOperands(object)));
   } else if (type == "|") {
       return std::unique_ptr<Operation>(new FoldingOperation<
               std::logical_or<bool>, translator::Bool>(
               parseOperands(object)));
   } else if (type == "!") {
       return std::unique_ptr<Operation>(new UnaryOperation<
               std::logical_not<bool>, translator::Bool>(
                       doParse(object["op"])));
   } else if (type == "if") {
       return std::unique_ptr<Operation>(new Conditional(
                       doParse(object["cond"]),
                       doParse(object["then"]),
                       doParse(object["else"])));
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

} // namespace operation
