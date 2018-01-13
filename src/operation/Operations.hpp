#ifndef OPERATION_OPERATIONS_HPP
#define OPERATION_OPERATIONS_HPP

#include "Operation.hpp"
#include "Translator.hpp"
#include "../common/InterfaceConfig.hpp"

#include <algorithm>
#include <numeric>
#include <string>

namespace operation {

class Constant : public Operation {
public:
   explicit Constant(const std::string& value) : value(value) {}
   std::string evaluate() override;

private:
   std::string value;
};

class Value : public Operation {
public:
   Value(const InterfaceConfig* interface, std::size_t index)
           : interface(interface), index(index) {}

   std::string evaluate() override;

private:
   const InterfaceConfig* interface;
   std::size_t index;
};

class Template : public Operation {
public:
   Template(const InterfaceConfig* interface, const std::string& template_)
           : interface(interface), template_(template_) {}

   std::string evaluate() override;
private:
   const InterfaceConfig* interface;
   std::string template_;
};

class Conditional : public Operation {
public:
   Conditional(std::unique_ptr<Operation>&& condition,
           std::unique_ptr<Operation>&& then,
           std::unique_ptr<Operation>&& else_)
           : condition(std::move(condition)),
             then(std::move(then)),
             else_(std::move(else_)) {}

   std::string evaluate() override;
private:
   std::unique_ptr<Operation> condition;
   std::unique_ptr<Operation> then;
   std::unique_ptr<Operation> else_;
};

template<typename Operator, typename Translator>
class FoldingOperation : public Operation {
public:
   FoldingOperation(std::vector<std::unique_ptr<Operation>> operands,
           Operator operator_ = Operator(),
           Translator translator = Translator())
           : operands(std::move(operands)),
             operator_(std::move(operator_)),
             translator(std::move(translator)) {}

   std::string evaluate() override {
       using Type = decltype(translator.fromString(
               std::declval<std::string>()));
       if (operands.empty()) {
           return translator.toString(Type{});
       }
       return translator.toString(std::accumulate(
               operands.begin() + 1, operands.end(),
               translator.fromString(operands.front()->evaluate()),
               [this](const Type& lhs, const std::unique_ptr<Operation>& rhs) {
                   std::string value = rhs->evaluate();
                   auto translated = translator.fromString(value);
                   auto result = operator_(lhs, translated);
                   // std::cerr << "(" << lhs << ", [" << value << "="
                   //         << translated << "]) = " << result << "\n";
                   return result;
               }));
   }

private:
   std::vector<std::unique_ptr<Operation>> operands;
   Operator operator_;
   Translator translator;
};

template<typename Operator, typename Translator>
class Comparison : public Operation {
public:
   Comparison(std::vector<std::unique_ptr<Operation>> operands,
           Operator operator_ = Operator(),
           Translator translator = Translator())
           : operands(std::move(operands)),
             operator_(std::move(operator_)),
             translator(std::move(translator)) {}

   std::string evaluate() override {
       return translator::Bool{}.toString(std::adjacent_find(
               operands.begin(), operands.end(),
               [this](const std::unique_ptr<Operation>& lhs,
                       const std::unique_ptr<Operation>& rhs) {
                   return !operator_(
                           translator.fromString(lhs->evaluate()),
                           translator.fromString(rhs->evaluate()));
               }) == operands.end());
   }

private:
   std::vector<std::unique_ptr<Operation>> operands;
   Operator operator_;
   Translator translator;
};

template<typename Operator, typename Translator>
class UnaryOperation : public Operation {
public:
   UnaryOperation(std::unique_ptr<Operation> operand,
           Operator operator_ = Operator(),
           Translator translator = Translator())
           : operand(std::move(operand)),
             operator_(std::move(operator_)),
             translator(std::move(translator)) {}

   std::string evaluate() override {
       return translator.toString(
               operator_(translator.fromString(operand->evaluate())));
   }

private:
   std::unique_ptr<Operation> operand;
   Operator operator_;
   Translator translator;
};

} // namespace operation

#endif // OPERATION_OPERATIONS_HPP
