#include "OperationParser2.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <functional>

#include "../common/InterfaceConfig.hpp"
#include "Operations.hpp"
#include "Translator.hpp"

namespace operation {

namespace {

std::string unescapeString(const std::string& s) {
    std::string result;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
            case '\'':
                result.push_back('\'');
                break;
            case '\\':
                result.push_back('\\');
                break;
            default:
                result.push_back(s[i]);
                result.push_back(s[i + 1]);
                break;
            }
            ++i;
        } else {
            result.push_back(s[i]);
        }
    }
    return result;
}

std::vector<std::unique_ptr<Operation>> makeOperands(
    std::unique_ptr<Operation> left, std::unique_ptr<Operation> right) {
    std::vector<std::unique_ptr<Operation>> operands;
    operands.push_back(std::move(left));
    operands.push_back(std::move(right));
    return operands;
}

class Impl {
public:
    Impl(
        std::ostream& debug, const std::vector<InterfaceConfig*>& interfaces,
        InterfaceConfig* defaultInterface)
        : debug(debug)
        , interfaces(interfaces)
        , defaultInterface(defaultInterface) {}

    std::unique_ptr<Operation> parse(const std::string& data) {
        this->pos = 0;
        this->data = data;
        this->skipWhitespace();
        if (this->pos >= data.size()) {
            this->debug << "Syntax error: Empty expression" << std::endl;
            return nullptr;
        }
        auto result = this->parseExpression();
        if (result) {
            this->skipWhitespace();
            if (this->pos != data.size()) {
                this->debug << "Syntax error: Unfinished expression"
                            << std::endl;
                return nullptr;
            }
        }
        return result;
    }

    std::unordered_set<InterfaceConfig*>&& getUsedInterfaces() && {
        return std::move(this->usedInterfaces);
    }

private:
    std::ostream& debug;
    const std::vector<InterfaceConfig*>& interfaces;
    InterfaceConfig* const defaultInterface;
    std::unordered_set<InterfaceConfig*> usedInterfaces;

    std::string data;
    size_t pos = 0;

    void skipWhitespace() {
        while (this->pos < this->data.size() &&
               std::isspace(this->data[this->pos])) {
            ++this->pos;
        }
    }

    bool match(char c) {
        this->skipWhitespace();
        if (this->pos < this->data.size() && this->data[this->pos] == c) {
            ++this->pos;
            return true;
        }
        return false;
    }

    bool match(const std::string& s) {
        this->skipWhitespace();
        if (this->pos + s.size() <= this->data.size() &&
            this->data.substr(this->pos, s.size()) == s) {
            this->pos += s.size();
            return true;
        }
        return false;
    }

    std::unique_ptr<Operation> parseExpression() {
        auto condition = this->parseOr();
        if (!condition) {
            return nullptr;
        }
        this->skipWhitespace();
        if (this->match('?')) {
            auto then = this->parseExpression();
            if (!then) {
                return nullptr;
            }
            if (!this->match(':')) {
                this->debug
                    << "Syntax error: Expected ':' in conditional expression"
                    << std::endl;
                return nullptr;
            }
            auto else_ = this->parseExpression();
            if (!else_) {
                return nullptr;
            }
            return std::make_unique<Conditional>(
                std::move(condition), std::move(then), std::move(else_));
        }
        return condition;
    }

    std::unique_ptr<Operation> parseOr() {
        auto left = this->parseAnd();
        if (!left) {
            return nullptr;
        }
        while (true) {
            this->skipWhitespace();
            if (this->match("||")) {
                auto right = this->parseAnd();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    FoldingOperation<std::logical_or<bool>, translator::Bool>>(
                    makeOperands(std::move(left), std::move(right)));
            } else {
                break;
            }
        }
        return left;
    }

    std::unique_ptr<Operation> parseAnd() {
        auto left = this->parseEquality();
        if (!left) {
            return nullptr;
        }
        while (true) {
            this->skipWhitespace();
            if (this->match("&&")) {
                auto right = this->parseEquality();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    FoldingOperation<std::logical_and<bool>, translator::Bool>>(
                    makeOperands(std::move(left), std::move(right)));
            } else {
                break;
            }
        }
        return left;
    }

    std::unique_ptr<Operation> parseEquality() {
        auto left = this->parseComparison();
        if (!left) {
            return nullptr;
        }
        while (true) {
            this->skipWhitespace();
            std::unique_ptr<Operation> right;
            if (this->match("==")) {
                right = this->parseComparison();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::equal_to<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (this->match("!=")) {
                right = this->parseComparison();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::not_equal_to<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (this->match("s==")) {
                right = this->parseComparison();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::equal_to<std::string>, translator::Str>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (this->match("s!=")) {
                right = this->parseComparison();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<Comparison<
                    std::not_equal_to<std::string>, translator::Str>>(
                    makeOperands(std::move(left), std::move(right)));
            } else {
                break;
            }
        }
        return left;
    }

    std::unique_ptr<Operation> parseComparison() {
        auto left = this->parseTerm();
        if (!left) {
            return nullptr;
        }
        while (true) {
            this->skipWhitespace();
            std::unique_ptr<Operation> right;
            if (this->match("s<=")) {
                right = this->parseTerm();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::less_equal<std::string>, translator::Str>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (this->match("s>=")) {
                right = this->parseTerm();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<Comparison<
                    std::greater_equal<std::string>, translator::Str>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (this->match("s<")) {
                right = this->parseTerm();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::less<std::string>, translator::Str>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (this->match("s>")) {
                right = this->parseTerm();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::greater<std::string>, translator::Str>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (this->match("<=")) {
                right = this->parseTerm();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::less_equal<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (this->match(">=")) {
                right = this->parseTerm();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::greater_equal<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (this->match('<')) {
                right = this->parseTerm();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::less<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (this->match('>')) {
                right = this->parseTerm();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::greater<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else {
                break;
            }
        }
        return left;
    }

    std::unique_ptr<Operation> parseTerm() {
        auto left = this->parseFactor();
        if (!left) {
            return nullptr;
        }
        while (true) {
            this->skipWhitespace();
            std::unique_ptr<Operation> right;
            if (this->match('+')) {
                right = this->parseFactor();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    FoldingOperation<std::plus<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (this->match('-')) {
                right = this->parseFactor();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    FoldingOperation<std::minus<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (this->match("s+")) {
                right = this->parseFactor();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    FoldingOperation<std::plus<std::string>, translator::Str>>(
                    makeOperands(std::move(left), std::move(right)));
            } else {
                break;
            }
        }
        return left;
    }

    std::unique_ptr<Operation> parseFactor() {
        auto left = this->parseUnary();
        if (!left) {
            return nullptr;
        }
        while (true) {
            this->skipWhitespace();
            std::unique_ptr<Operation> right;
            if (this->match('*')) {
                right = this->parseUnary();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<FoldingOperation<
                    std::multiplies<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (this->match('/')) {
                right = this->parseUnary();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    FoldingOperation<std::divides<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else {
                break;
            }
        }
        return left;
    }

    std::unique_ptr<Operation> parseUnary() {
        this->skipWhitespace();
        if (this->match('!')) {
            auto operand = this->parseUnary();
            if (!operand) {
                return nullptr;
            }
            return std::make_unique<
                UnaryOperation<std::logical_not<bool>, translator::Bool>>(
                std::move(operand));
        }
        if (this->match('-')) {
            auto operand = this->parseUnary();
            if (!operand) {
                return nullptr;
            }
            std::vector<std::unique_ptr<Operation>> operands;
            operands.push_back(std::make_unique<Constant>("0"));
            operands.push_back(std::move(operand));
            return std::make_unique<
                FoldingOperation<std::minus<float>, translator::Float>>(
                std::move(operands));
        }
        return this->parsePrimary();
    }

    std::unique_ptr<Operation> parsePrimary() {
        this->skipWhitespace();
        if (this->match('(')) {
            auto expr = this->parseExpression();
            if (!expr) {
                return nullptr;
            }
            this->skipWhitespace();
            if (!this->match(')')) {
                this->debug << "Syntax error: Unmatched closing parenthesis"
                            << std::endl;
                return nullptr;
            }
            return expr;
        }

        if (this->match('\'')) {
            return this->parseStringLiteral();
        }

        if (this->match("true") || this->match("on")) {
            return std::make_unique<Constant>("1");
        }

        if (this->match("false") || this->match("off")) {
            return std::make_unique<Constant>("0");
        }

        if (this->match('[')) {
            return this->parseInterfaceValue();
        }

        if (this->match('%')) {
            return this->parseDefaultInterfaceValue();
        }

        return this->parseNumber();
    }

    std::unique_ptr<Operation> parseStringLiteral() {
        std::string value;
        while (this->pos < this->data.size()) {
            if (this->data[this->pos] == '\\' &&
                this->pos + 1 < this->data.size()) {
                value += this->data[this->pos];
                ++this->pos;
                if (this->pos < this->data.size()) {
                    value += this->data[this->pos];
                    ++this->pos;
                }
            } else if (this->data[this->pos] == '\'') {
                ++this->pos;
                return std::make_unique<Constant>(unescapeString(value));
            } else {
                value += this->data[this->pos];
                ++this->pos;
            }
        }
        this->debug << "Syntax error: Unmatched quote" << std::endl;
        return nullptr;
    }

    std::unique_ptr<Operation> parseNumber() {
        this->skipWhitespace();
        size_t start = this->pos;
        if (this->pos < this->data.size() &&
            (this->data[this->pos] == '-' || this->data[this->pos] == '+')) {
            ++this->pos;
        }
        bool hasDigit = false;
        while (this->pos < this->data.size() &&
               (std::isdigit(this->data[this->pos]) ||
                this->data[this->pos] == '.')) {
            if (std::isdigit(this->data[this->pos])) {
                hasDigit = true;
            }
            ++this->pos;
        }
        if (!hasDigit) {
            this->debug << "Syntax error: Expected number" << std::endl;
            return nullptr;
        }
        return std::make_unique<Constant>(
            this->data.substr(start, this->pos - start));
    }

    std::unique_ptr<Operation> parseInterfaceValue() {
        std::string name;
        while (this->pos < this->data.size() && this->data[this->pos] != ']') {
            if (this->data[this->pos] == '\\' &&
                this->pos + 1 < this->data.size()) {
                ++this->pos;
                if (this->pos < this->data.size()) {
                    name += this->data[this->pos];
                    ++this->pos;
                }
            } else {
                name += this->data[this->pos];
                ++this->pos;
            }
        }

        if (!this->match(']')) {
            this->debug << "Syntax error: Unmatched closing bracket"
                        << std::endl;
            return nullptr;
        }

        std::size_t index = 1;
        if (this->match('.')) {
            this->skipWhitespace();
            size_t indexStart = this->pos;
            while (this->pos < this->data.size() &&
                   std::isdigit(this->data[this->pos])) {
                ++this->pos;
            }
            if (indexStart == this->pos) {
                this->debug << "Syntax error: Bad value number" << std::endl;
                return nullptr;
            }
            std::string indexStr =
                this->data.substr(indexStart, this->pos - indexStart);
            char* endPtr;
            errno = 0;
            index = std::strtoul(indexStr.c_str(), &endPtr, 10);
            if (errno != 0 || endPtr != indexStr.c_str() + indexStr.size()) {
                this->debug << "Syntax error: Bad value number" << std::endl;
                return nullptr;
            }
        }

        InterfaceConfig* interface = nullptr;
        for (const auto& itf : this->interfaces) {
            if (itf && itf->name == name) {
                interface = itf;
                break;
            }
        }

        if (!interface) {
            this->debug << "Error: Interface not found: " << name << std::endl;
            return nullptr;
        }

        this->usedInterfaces.insert(interface);
        return std::make_unique<Value>(interface, index);
    }

    std::unique_ptr<Operation> parseDefaultInterfaceValue() {
        size_t start = this->pos;
        while (this->pos < this->data.size() &&
               std::isdigit(this->data[this->pos])) {
            ++this->pos;
        }
        if (start == this->pos) {
            this->debug << "Syntax error: Expected digit after '%'"
                        << std::endl;
            return nullptr;
        }

        std::string indexStr = this->data.substr(start, this->pos - start);
        char* endPtr;
        errno = 0;
        std::size_t index = std::strtoul(indexStr.c_str(), &endPtr, 10);
        if (errno != 0 || endPtr != indexStr.c_str() + indexStr.size()) {
            this->debug << "Syntax error: Bad value number" << std::endl;
            return nullptr;
        }
        if (!this->defaultInterface) {
            this->debug << "Error: No default interface" << std::endl;
            return nullptr;
        }
        this->usedInterfaces.insert(this->defaultInterface);
        return std::make_unique<Value>(this->defaultInterface, index);
    }
};

}  // anonymous namespace

Parser2::Parser2(
    std::ostream& debug,
    const std::vector<std::unique_ptr<InterfaceConfig>>& interfaces,
    InterfaceConfig* defaultInterface)
    : debug(debug)
    , interfaces(interfaces.size(), nullptr)
    , defaultInterface(defaultInterface) {
    std::transform(
        interfaces.begin(), interfaces.end(), this->interfaces.begin(),
        [](const std::unique_ptr<InterfaceConfig>& interface) {
        return interface.get();
    });
}

std::unique_ptr<Operation> Parser2::parse(const std::string& data) {
    Impl parser(this->debug, this->interfaces, this->defaultInterface);
    auto result = parser.parse(data);
    this->usedInterfaces = std::move(parser).getUsedInterfaces();
    return result;
}

}  // namespace operation
