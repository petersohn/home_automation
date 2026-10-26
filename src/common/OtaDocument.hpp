#ifndef COMMON_OTA_DOCUMENT_HPP
#define COMMON_OTA_DOCUMENT_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "ArduinoJson.hpp"
#include "OtaFile.hpp"

struct OtaConfigPolicy {
    bool valid = false;
    uint32_t version = 0;
    std::string servicePrefix;
    std::string reason;
};

OtaConfigPolicy inspectOtaConfig(
    const ArduinoJson::JsonObject& config, bool requireOta,
    bool allowLegacyVersion);

struct ParsedOtaDocument {
    std::unique_ptr<ArduinoJson::DynamicJsonBuffer> buffer;
    ArduinoJson::JsonObject* config = nullptr;
    OtaConfigPolicy policy;
};

ParsedOtaDocument parseOtaDocument(
    OtaFile& file, const std::string& localMac,
    std::optional<uint32_t> minimum);

#endif  // COMMON_OTA_DOCUMENT_HPP
