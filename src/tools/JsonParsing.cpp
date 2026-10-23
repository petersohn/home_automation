#include "JsonParsing.hpp"

#include <cstddef>

namespace tools {

struct CountedJsonInput {
    explicit CountedJsonInput(const std::string& value) : text(value) {}

    const std::string& text;
    std::size_t position = 0;
};

}  // namespace tools

namespace ArduinoJson {
namespace Internals {

template <>
struct StringTraits<tools::CountedJsonInput, void> {
    class Reader {
    public:
        explicit Reader(tools::CountedJsonInput& input) : input(input) {}

        void move() {
            if (this->input.position < this->input.text.size()) {
                ++this->input.position;
            }
        }

        char current() const { return this->characterAt(this->input.position); }

        char next() const {
            return this->characterAt(this->input.position + 1);
        }

    private:
        char characterAt(std::size_t position) const {
            if (position >= this->input.text.size()) {
                return '\0';
            }
            return this->input.text[position];
        }

        tools::CountedJsonInput& input;
    };

    static const bool has_append = false;
    static const bool has_equals = false;
};

}  // namespace Internals
}  // namespace ArduinoJson

namespace tools {
namespace {

bool hasOnlyTrailingWhitespace(const CountedJsonInput& input) {
    for (std::size_t i = input.position; i < input.text.size(); ++i) {
        const char value = input.text[i];
        if (value != ' ' && value != '\t' && value != '\n' && value != '\r') {
            return false;
        }
    }
    return true;
}

}  // namespace

ArduinoJson::JsonVariant parseFullJson(
    const std::string& text, ArduinoJson::DynamicJsonBuffer& buffer,
    uint8_t maxNestingDepth) {
    if (text.find('\0') != std::string::npos) {
        return ArduinoJson::JsonVariant();
    }

    CountedJsonInput input(text);
    ArduinoJson::JsonVariant parsed = buffer.parse(input, maxNestingDepth);
    if (!parsed.success() || !hasOnlyTrailingWhitespace(input)) {
        return ArduinoJson::JsonVariant();
    }
    return parsed;
}

bool parseUint32(const ArduinoJson::JsonVariant& value, uint32_t& result) {
    if (!value.is<ArduinoJson::Internals::JsonInteger>()) {
        return false;
    }
    const char* token = value.as<const char*>();
    if (token == nullptr || *token == '\0') {
        return false;
    }
    const bool negative = *token == '-';
    if (*token == '-' || *token == '+') {
        ++token;
    }
    if (*token == '\0') {
        return false;
    }
    uint32_t parsed = 0;
    for (; *token != '\0'; ++token) {
        const char digit = *token;
        if (digit < '0' || digit > '9') {
            return false;
        }
        const uint32_t digitValue = static_cast<uint32_t>(digit - '0');
        if (parsed > (UINT32_MAX - digitValue) / 10) {
            return false;
        }
        parsed = parsed * 10 + digitValue;
    }
    if (negative && parsed != 0) {
        return false;
    }
    result = parsed;
    return true;
}

}  // namespace tools
