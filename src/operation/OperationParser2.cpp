#include "OperationParser2.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <sstream>

#include "../common/Interface.hpp"
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
        pos = 0;
        this->data = data;
        skipWhitespace();
        if (pos >= data.size()) {
            debug << "Syntax error: Empty expression" << std::endl;
            return nullptr;
        }
        auto result = parseExpression();
        if (result) {
            skipWhitespace();
            if (pos != data.size()) {
                debug << "Syntax error: Unfinished expression" << std::endl;
                return nullptr;
            }
        }
        return result;
    }

    std::unordered_set<const InterfaceConfig*>&& getUsedInterfaces() && {
        return std::move(usedInterfaces);
    }

private:
    std::ostream& debug;
    const std::vector<InterfaceConfig*>& interfaces;
    InterfaceConfig* const defaultInterface;
    std::unordered_set<const InterfaceConfig*> usedInterfaces;

    std::string data;
    size_t pos = 0;

    void skipWhitespace() {
        while (pos < data.size() && std::isspace(data[pos])) {
            ++pos;
        }
    }

    bool match(char c) {
        skipWhitespace();
        if (pos < data.size() && data[pos] == c) {
            ++pos;
            return true;
        }
        return false;
    }

    bool match(const std::string& s) {
        skipWhitespace();
        if (pos + s.size() <= data.size() && data.substr(pos, s.size()) == s) {
            pos += s.size();
            return true;
        }
        return false;
    }

    char peek() {
        skipWhitespace();
        if (pos < data.size()) {
            return data[pos];
        }
        return '\0';
    }

    std::unique_ptr<Operation> parseExpression() {
        auto condition = parseOr();
        if (!condition) {
            return nullptr;
        }
        skipWhitespace();
        if (match('?')) {
            auto then = parseExpression();
            if (!then) {
                return nullptr;
            }
            if (!match(':')) {
                debug << "Syntax error: Expected ':' in conditional expression"
                      << std::endl;
                return nullptr;
            }
            auto else_ = parseExpression();
            if (!else_) {
                return nullptr;
            }
            return std::make_unique<Conditional>(
                std::move(condition), std::move(then), std::move(else_));
        }
        return condition;
    }

    std::unique_ptr<Operation> parseOr() {
        auto left = parseAnd();
        if (!left) {
            return nullptr;
        }
        while (true) {
            skipWhitespace();
            if (match("||")) {
                auto right = parseAnd();
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
        auto left = parseEquality();
        if (!left) {
            return nullptr;
        }
        while (true) {
            skipWhitespace();
            if (match("&&")) {
                auto right = parseEquality();
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
        auto left = parseComparison();
        if (!left) {
            return nullptr;
        }
        while (true) {
            skipWhitespace();
            std::unique_ptr<Operation> right;
            if (match("==")) {
                right = parseComparison();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::equal_to<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (match("!=")) {
                right = parseComparison();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::not_equal_to<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (match("s==")) {
                right = parseComparison();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::equal_to<std::string>, translator::Str>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (match("s!=")) {
                right = parseComparison();
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
        auto left = parseTerm();
        if (!left) {
            return nullptr;
        }
        while (true) {
            skipWhitespace();
            std::unique_ptr<Operation> right;
            if (match("s<=")) {
                right = parseTerm();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::less_equal<std::string>, translator::Str>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (match("s>=")) {
                right = parseTerm();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<Comparison<
                    std::greater_equal<std::string>, translator::Str>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (match("s<")) {
                right = parseTerm();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::less<std::string>, translator::Str>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (match("s>")) {
                right = parseTerm();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::greater<std::string>, translator::Str>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (match("<=")) {
                right = parseTerm();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::less_equal<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (match(">=")) {
                right = parseTerm();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::greater_equal<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (match('<')) {
                right = parseTerm();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    Comparison<std::less<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (match('>')) {
                right = parseTerm();
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
        auto left = parseFactor();
        if (!left) {
            return nullptr;
        }
        while (true) {
            skipWhitespace();
            std::unique_ptr<Operation> right;
            if (match('+')) {
                right = parseFactor();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    FoldingOperation<std::plus<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (match('-')) {
                right = parseFactor();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<
                    FoldingOperation<std::minus<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (match("s+")) {
                right = parseFactor();
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
        auto left = parseUnary();
        if (!left) {
            return nullptr;
        }
        while (true) {
            skipWhitespace();
            std::unique_ptr<Operation> right;
            if (match('*')) {
                right = parseUnary();
                if (!right) {
                    return nullptr;
                }
                left = std::make_unique<FoldingOperation<
                    std::multiplies<float>, translator::Float>>(
                    makeOperands(std::move(left), std::move(right)));
            } else if (match('/')) {
                right = parseUnary();
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
        skipWhitespace();
        if (match('!')) {
            auto operand = parseUnary();
            if (!operand) {
                return nullptr;
            }
            return std::make_unique<
                UnaryOperation<std::logical_not<bool>, translator::Bool>>(
                std::move(operand));
        }
        if (match('-')) {
            auto operand = parseUnary();
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
        return parsePrimary();
    }

    std::unique_ptr<Operation> parsePrimary() {
        skipWhitespace();
        if (match('(')) {
            auto expr = parseExpression();
            if (!expr) {
                return nullptr;
            }
            skipWhitespace();
            if (!match(')')) {
                debug << "Syntax error: Unmatched closing parenthesis"
                      << std::endl;
                return nullptr;
            }
            return expr;
        }

        if (match('\'')) {
            return parseStringLiteral();
        }

        if (match("true")) {
            return std::make_unique<Constant>("1");
        }

        if (match("false")) {
            return std::make_unique<Constant>("0");
        }

        if (match('[')) {
            return parseInterfaceValue();
        }

        if (match('%')) {
            return parseDefaultInterfaceValue();
        }

        return parseNumber();
    }

    std::unique_ptr<Operation> parseStringLiteral() {
        std::string value;
        while (pos < data.size()) {
            if (data[pos] == '\\' && pos + 1 < data.size()) {
                value += data[pos];
                ++pos;
                if (pos < data.size()) {
                    value += data[pos];
                    ++pos;
                }
            } else if (data[pos] == '\'') {
                ++pos;
                return std::make_unique<Constant>(unescapeString(value));
            } else {
                value += data[pos];
                ++pos;
            }
        }
        debug << "Syntax error: Unmatched quote" << std::endl;
        return nullptr;
    }

    std::unique_ptr<Operation> parseNumber() {
        skipWhitespace();
        size_t start = pos;
        if (pos < data.size() && (data[pos] == '-' || data[pos] == '+')) {
            ++pos;
        }
        bool hasDigit = false;
        while (pos < data.size() &&
               (std::isdigit(data[pos]) || data[pos] == '.')) {
            if (std::isdigit(data[pos])) {
                hasDigit = true;
            }
            ++pos;
        }
        if (!hasDigit) {
            debug << "Syntax error: Expected number" << std::endl;
            return nullptr;
        }
        return std::make_unique<Constant>(data.substr(start, pos - start));
    }

    std::unique_ptr<Operation> parseInterfaceValue() {
        std::string name;
        while (pos < data.size() && data[pos] != ']') {
            if (data[pos] == '\\' && pos + 1 < data.size()) {
                ++pos;
                if (pos < data.size()) {
                    name += data[pos];
                    ++pos;
                }
            } else {
                name += data[pos];
                ++pos;
            }
        }

        if (!match(']')) {
            debug << "Syntax error: Unmatched closing bracket" << std::endl;
            return nullptr;
        }

        std::size_t index = 1;
        if (match('.')) {
            skipWhitespace();
            size_t indexStart = pos;
            while (pos < data.size() && std::isdigit(data[pos])) {
                ++pos;
            }
            if (indexStart == pos) {
                debug << "Syntax error: Bad value number" << std::endl;
                return nullptr;
            }
            try {
                index = std::stoul(data.substr(indexStart, pos - indexStart));
            } catch (...) {
                debug << "Syntax error: Bad value number" << std::endl;
                return nullptr;
            }
        }

        const InterfaceConfig* interface = nullptr;
        for (const auto& itf : interfaces) {
            if (itf && itf->name == name) {
                interface = itf;
                break;
            }
        }

        if (!interface) {
            debug << "Error: Interface not found: " << name << std::endl;
            return nullptr;
        }

        usedInterfaces.insert(interface);
        return std::make_unique<Value>(interface, index);
    }

    std::unique_ptr<Operation> parseDefaultInterfaceValue() {
        size_t start = pos;
        while (pos < data.size() && std::isdigit(data[pos])) {
            ++pos;
        }
        if (start == pos) {
            debug << "Syntax error: Expected digit after '%'" << std::endl;
            return nullptr;
        }

        try {
            std::size_t index = std::stoul(data.substr(start, pos - start));
            if (!defaultInterface) {
                debug << "Error: No default interface" << std::endl;
                return nullptr;
            }
            usedInterfaces.insert(defaultInterface);
            return std::make_unique<Value>(defaultInterface, index);
        } catch (...) {
            debug << "Syntax error: Bad value number" << std::endl;
            return nullptr;
        }
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
    Impl parser(debug, interfaces, defaultInterface);
    return parser.parse(data);
}

}  // namespace operation
