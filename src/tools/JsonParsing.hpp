#ifndef TOOLS_JSONPARSING_HPP
#define TOOLS_JSONPARSING_HPP

#include <cstdint>
#include <string>
#include <utility>

#include "../common/ArduinoJson.hpp"

namespace tools {

namespace detail {

class JsonStreamInput {
public:
    using ReadCallback = int (*)(void*);

    JsonStreamInput(void* source, ReadCallback readCallback);

    char current();
    char next();
    void move();
    bool hasNul() const;

private:
    int read();
    int currentValue();
    int nextValue();

    void* source;
    ReadCallback readCallback;
    int currentCharacter;
    int nextCharacter;
    bool nulEncountered;
};

}  // namespace detail

ArduinoJson::JsonVariant parseFullJson(
    const std::string& text, ArduinoJson::DynamicJsonBuffer& buffer,
    uint8_t maxNestingDepth);

ArduinoJson::JsonVariant parseFullJson(
    detail::JsonStreamInput& input, ArduinoJson::DynamicJsonBuffer& buffer,
    uint8_t maxNestingDepth);

template <
    typename TStream,
    typename = decltype(static_cast<int>(std::declval<TStream&>().read()))>
ArduinoJson::JsonVariant parseFullJson(
    TStream& stream, ArduinoJson::DynamicJsonBuffer& buffer,
    uint8_t maxNestingDepth) {
    const auto readCallback = [](void* source) {
        return static_cast<int>(static_cast<TStream*>(source)->read());
    };
    detail::JsonStreamInput input(&stream, readCallback);
    return parseFullJson(input, buffer, maxNestingDepth);
}

bool parseUint32(const ArduinoJson::JsonVariant& value, uint32_t& result);

}  // namespace tools

#endif  // TOOLS_JSONPARSING_HPP
