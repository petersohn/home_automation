#include <optional>

#include "../common/ArduinoJson.hpp"

namespace tools {

template <typename T>
std::optional<T> fromString(const std::string& s) {
    ArduinoJson::StaticJsonBuffer<20> buf;
    auto json = buf.parse(s);
    return json.is<T>() ? std::make_optional<T>(json.as<T>()) : std::nullopt;
}

}  // namespace tools
