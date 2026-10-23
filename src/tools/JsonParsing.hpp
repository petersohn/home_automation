#ifndef TOOLS_JSONPARSING_HPP
#define TOOLS_JSONPARSING_HPP

#include <cstdint>
#include <string>

#include "../common/ArduinoJson.hpp"

namespace tools {

ArduinoJson::JsonVariant parseFullJson(
    const std::string& text, ArduinoJson::DynamicJsonBuffer& buffer,
    uint8_t maxNestingDepth);

bool parseUint32(const ArduinoJson::JsonVariant& value, uint32_t& result);

}  // namespace tools

#endif  // TOOLS_JSONPARSING_HPP
