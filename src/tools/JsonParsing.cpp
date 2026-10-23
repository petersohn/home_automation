#include "JsonParsing.hpp"

#include <cstddef>

namespace tools {

struct CountedJsonInput {
    explicit CountedJsonInput(const std::string& value) : text(value) {}

    const std::string& text;
    std::size_t position = 0;
};

}  // namespace tools

tools::detail::JsonStreamInput::JsonStreamInput(
    void* source, ReadCallback readCallback)
    : source(source)
    , readCallback(readCallback)
    , currentCharacter(-2)
    , nextCharacter(-2)
    , nulEncountered(false) {}

int tools::detail::JsonStreamInput::read() {
    const int character = this->readCallback(this->source);
    if (character == 0) {
        this->nulEncountered = true;
    }
    return character;
}

int tools::detail::JsonStreamInput::currentValue() {
    if (this->currentCharacter == -2) {
        this->currentCharacter = this->read();
    }
    return this->currentCharacter;
}

int tools::detail::JsonStreamInput::nextValue() {
    if (this->currentValue() < 0) {
        return -1;
    }
    if (this->nextCharacter == -2) {
        this->nextCharacter = this->read();
    }
    return this->nextCharacter;
}

char tools::detail::JsonStreamInput::current() {
    const int character = this->currentValue();
    return character < 0 ? '\0' : static_cast<char>(character);
}

char tools::detail::JsonStreamInput::next() {
    const int character = this->nextValue();
    return character < 0 ? '\0' : static_cast<char>(character);
}

void tools::detail::JsonStreamInput::move() {
    if (this->currentValue() < 0) {
        return;
    }
    this->currentCharacter =
        this->nextCharacter == -2 ? this->read() : this->nextCharacter;
    this->nextCharacter = -2;
}

bool tools::detail::JsonStreamInput::hasNul() const {
    return this->nulEncountered;
}

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

template <>
struct StringTraits<tools::detail::JsonStreamInput, void> {
    class Reader {
    public:
        explicit Reader(tools::detail::JsonStreamInput& input) : input(input) {}

        void move() { this->input.move(); }
        char current() { return this->input.current(); }
        char next() { return this->input.next(); }

    private:
        tools::detail::JsonStreamInput& input;
    };

    static const bool has_append = false;
    static const bool has_equals = false;
};

}  // namespace Internals
}  // namespace ArduinoJson

namespace tools {
namespace {

bool isJsonWhitespace(char value) {
    return value == ' ' || value == '\t' || value == '\n' || value == '\r';
}

bool hasOnlyTrailingWhitespace(const CountedJsonInput& input) {
    for (std::size_t i = input.position; i < input.text.size(); ++i) {
        if (!isJsonWhitespace(input.text[i])) {
            return false;
        }
    }
    return true;
}

bool hasOnlyTrailingWhitespace(detail::JsonStreamInput& input) {
    for (;;) {
        const char value = input.current();
        if (value == '\0') {
            return !input.hasNul();
        }
        if (!isJsonWhitespace(value)) {
            return false;
        }
        input.move();
    }
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

ArduinoJson::JsonVariant parseFullJson(
    detail::JsonStreamInput& input, ArduinoJson::DynamicJsonBuffer& buffer,
    uint8_t maxNestingDepth) {
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
