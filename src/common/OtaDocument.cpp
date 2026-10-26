#include "OtaDocument.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "../tools/JsonParsing.hpp"

namespace {

constexpr std::size_t maxOtaDocumentBytes = 32768;
constexpr std::size_t maxOtaServicePrefixBytes = 482;
constexpr uint8_t otaEnvelopeNestingLimit =
    ARDUINOJSON_DEFAULT_NESTING_LIMIT + 1;

OtaConfigPolicy invalidPolicy(const char* reason) {
    OtaConfigPolicy result;
    result.reason = reason;
    return result;
}

bool parseMac(const std::string& text, std::array<uint8_t, 6>& result) {
    if (text.size() != 17) {
        return false;
    }

    auto hexValue = [](char value) -> int {
        if (value >= '0' && value <= '9') {
            return value - '0';
        }
        if (value >= 'a' && value <= 'f') {
            return value - 'a' + 10;
        }
        if (value >= 'A' && value <= 'F') {
            return value - 'A' + 10;
        }
        return -1;
    };

    for (std::size_t i = 0; i < result.size(); ++i) {
        const std::size_t offset = i * 3;
        if (i != 0 && text[offset - 1] != ':') {
            return false;
        }
        const int high = hexValue(text[offset]);
        const int low = hexValue(text[offset + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        result[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

class BoundedOtaInput {
public:
    explicit BoundedOtaInput(OtaFile& file) : file(file) {}

    int read() {
        const int value = this->file.read();
        if (value == -1) {
            return value;
        }
        if (value < 0 || value > 255) {
            this->ioError = true;
            return -2;
        }
        if (this->bytesRead == maxOtaDocumentBytes) {
            this->tooLarge = true;
            return -2;
        }
        ++this->bytesRead;
        return value;
    }

    bool failed() const { return this->ioError || this->file.failed(); }
    bool exceededLimit() const { return this->tooLarge; }

private:
    OtaFile& file;
    std::size_t bytesRead = 0;
    bool ioError = false;
    bool tooLarge = false;
};

}  // namespace

const char* configSlotPath(ConfigSlot slot) {
    switch (slot) {
    case ConfigSlot::primary:
        return "/device_config.json";
    case ConfigSlot::secondary:
        return "/device_config_2.json";
    }
    return nullptr;
}

OtaConfigPolicy inspectOtaConfig(
    const ArduinoJson::JsonObject& config, bool requireOta,
    bool allowLegacyVersion) {
    OtaConfigPolicy result;
    const auto rawVersion =
        config.get<ArduinoJson::JsonVariant>("configVersion");
    if (config.containsKey("configVersion")) {
        if (!tools::parseUint32(rawVersion, result.version)) {
            return invalidPolicy("invalid-config-version");
        }
    } else if (!allowLegacyVersion) {
        return invalidPolicy("missing-config-version");
    }

    const auto rawPrefix =
        config.get<ArduinoJson::JsonVariant>("servicePrefix");
    const bool hasStringPrefix =
        rawPrefix.is<std::string>() && rawPrefix.as<const char*>() != nullptr;
    if (hasStringPrefix) {
        result.servicePrefix = rawPrefix.as<std::string>();
        while (!result.servicePrefix.empty() &&
               result.servicePrefix.back() == '/') {
            result.servicePrefix.pop_back();
        }
    }

    if (requireOta) {
        const auto rawEnabled =
            config.get<ArduinoJson::JsonVariant>("otaEnabled");
        if (!rawEnabled.is<bool>() || !rawEnabled.as<bool>()) {
            return invalidPolicy("ota-disabled");
        }
        if (!hasStringPrefix || result.servicePrefix.empty()) {
            return invalidPolicy("invalid-service-prefix");
        }
        if (result.servicePrefix.size() > maxOtaServicePrefixBytes) {
            return invalidPolicy("service-prefix-too-long");
        }
        for (char value : result.servicePrefix) {
            if (value == '\0' || value == '#' || value == '+') {
                return invalidPolicy("invalid-service-prefix");
            }
        }
    }

    result.valid = true;
    return result;
}

ParsedOtaDocument parseOtaDocument(
    OtaFile& file, const std::string& localMac,
    std::optional<uint32_t> minimum) {
    ParsedOtaDocument result;
    result.buffer = std::make_unique<ArduinoJson::DynamicJsonBuffer>(1024);
    BoundedOtaInput input(file);
    const auto parsed =
        tools::parseFullJson(input, *result.buffer, otaEnvelopeNestingLimit);
    if (input.failed() || file.failed()) {
        result.policy = invalidPolicy("file-read-error");
        return result;
    }
    if (input.exceededLimit()) {
        result.policy = invalidPolicy("document-too-large");
        return result;
    }
    if (!parsed.success() || !parsed.is<ArduinoJson::JsonObject>()) {
        result.policy = invalidPolicy("invalid-json");
        return result;
    }

    const auto& envelope = parsed.as<ArduinoJson::JsonObject>();
    if (envelope.size() != 3 || !envelope.containsKey("type") ||
        !envelope.containsKey("target") || !envelope.containsKey("config")) {
        result.policy = invalidPolicy("invalid-envelope");
        return result;
    }

    const auto rawType = envelope.get<ArduinoJson::JsonVariant>("type");
    const auto rawTarget = envelope.get<ArduinoJson::JsonVariant>("target");
    if (!rawType.is<const char*>() || !rawTarget.is<const char*>()) {
        result.policy = invalidPolicy("invalid-envelope");
        return result;
    }
    const char* const type = rawType.as<const char*>();
    const char* const target = rawTarget.as<const char*>();
    if (type == nullptr || target == nullptr ||
        std::string(type) != "device-config") {
        result.policy = invalidPolicy("invalid-envelope");
        return result;
    }

    std::array<uint8_t, 6> localMacBytes{};
    std::array<uint8_t, 6> targetMacBytes{};
    if (!parseMac(localMac, localMacBytes) ||
        !parseMac(target, targetMacBytes) || localMacBytes != targetMacBytes) {
        result.policy = invalidPolicy("target-mismatch");
        return result;
    }

    const auto rawConfig = envelope.get<ArduinoJson::JsonVariant>("config");
    if (!rawConfig.is<ArduinoJson::JsonObject>()) {
        result.policy = invalidPolicy("invalid-config");
        return result;
    }
    auto& config = rawConfig.as<ArduinoJson::JsonObject>();
    result.policy = inspectOtaConfig(config, true, false);
    if (!result.policy.valid) {
        return result;
    }
    if (!minimum.has_value()) {
        result.policy = invalidPolicy("version-range-exhausted");
        return result;
    }
    const uint32_t requiredVersion = *minimum < 1 ? 1 : *minimum;
    if (result.policy.version < requiredVersion) {
        result.policy = invalidPolicy("version-too-old");
        return result;
    }

    result.config = &config;
    return result;
}
