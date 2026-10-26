#include "UploadStatus.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

#include "../tools/JsonParsing.hpp"
#include "ArduinoJson.hpp"

namespace {

constexpr std::size_t maxMetadataBytes = 4096;
constexpr std::size_t maxHashBytes = 64;
constexpr std::size_t maxReasonBytes = 64;

bool isLowerHex(char value) {
    return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
}

bool isHash(const std::string& value) {
    if (value.size() != maxHashBytes) {
        return false;
    }
    for (char character : value) {
        if (!isLowerHex(character)) {
            return false;
        }
    }
    return true;
}

bool isLifecycleStatus(const std::string& status) {
    return status == "STAGED" || status == "CONFIRMED" ||
           status == "ROLLED_BACK";
}

bool isOtaResultStatus(const std::string& status) {
    return status == "PROGRESS" || status == "FAIL" || status == "ERROR" ||
           isLifecycleStatus(status);
}

bool isValidResult(const OtaResult& result, bool lifecycleOnly) {
    if (!isOtaResultStatus(result.status) ||
        (lifecycleOnly && !isLifecycleStatus(result.status)) ||
        result.reason.size() > maxReasonBytes) {
        return false;
    }
    for (unsigned char character : result.reason) {
        if (character < 0x20) {
            return false;
        }
    }
    if (!result.hash.empty() && !isHash(result.hash)) {
        return false;
    }
    if (isLifecycleStatus(result.status) &&
        (!result.version.has_value() || !isHash(result.hash))) {
        return false;
    }
    return true;
}

bool isValidSlot(ConfigSlot slot) {
    return slot == ConfigSlot::primary || slot == ConfigSlot::secondary;
}

bool isValidUploadStatus(const UploadStatus& state) {
    if (!isValidSlot(state.activeSlot)) {
        return false;
    }
    if (state.minimumAcceptedConfigVersion.has_value() &&
        (*state.minimumAcceptedConfigVersion == 0 ||
         state.activeVersion >= *state.minimumAcceptedConfigVersion)) {
        return false;
    }
    if (state.phase == UploadPhase::idle) {
        if (state.trialSlot.has_value() || state.trialVersion.has_value() ||
            !state.trialHash.empty() || state.trialBytes != 0) {
            return false;
        }
    } else if (
        state.phase == UploadPhase::pending ||
        state.phase == UploadPhase::started) {
        if (!state.trialSlot.has_value() || !state.trialVersion.has_value() ||
            !isValidSlot(*state.trialSlot) ||
            *state.trialSlot == state.activeSlot ||
            *state.trialVersion <= state.activeVersion ||
            !isHash(state.trialHash) ||
            (state.minimumAcceptedConfigVersion.has_value() &&
             *state.trialVersion >= *state.minimumAcceptedConfigVersion)) {
            return false;
        }
    } else {
        return false;
    }
    return !state.lastOutcome.has_value() ||
           isValidResult(*state.lastOutcome, true);
}

bool parseString(
    const ArduinoJson::JsonObject& object, const char* key,
    std::string& result) {
    const auto value = object.get<ArduinoJson::JsonVariant>(key);
    if (!value.is<const char*>()) {
        return false;
    }
    const char* const text = value.as<const char*>();
    if (text == nullptr) {
        return false;
    }
    result = text;
    return true;
}

bool isNull(const ArduinoJson::JsonVariant& value) {
    return value.is<const char*>() && value.as<const char*>() == nullptr;
}

bool parseInteger(
    const ArduinoJson::JsonObject& object, const char* key, uint32_t& result) {
    return tools::parseUint32(
        object.get<ArduinoJson::JsonVariant>(key), result);
}

bool parseOptionalInteger(
    const ArduinoJson::JsonObject& object, const char* key,
    std::optional<uint32_t>& result) {
    const auto value = object.get<ArduinoJson::JsonVariant>(key);
    if (isNull(value)) {
        result.reset();
        return true;
    }
    uint32_t parsed = 0;
    if (!tools::parseUint32(value, parsed)) {
        return false;
    }
    result = parsed;
    return true;
}

bool parseSlot(
    const ArduinoJson::JsonObject& object, const char* key,
    ConfigSlot& result) {
    uint32_t slot = 0;
    if (!parseInteger(object, key, slot) || (slot != 1 && slot != 2)) {
        return false;
    }
    result = static_cast<ConfigSlot>(slot);
    return true;
}

bool parseOptionalSlot(
    const ArduinoJson::JsonObject& object, const char* key,
    std::optional<ConfigSlot>& result) {
    const auto value = object.get<ArduinoJson::JsonVariant>(key);
    if (isNull(value)) {
        result.reset();
        return true;
    }
    uint32_t slot = 0;
    if (!tools::parseUint32(value, slot) || (slot != 1 && slot != 2)) {
        return false;
    }
    result = static_cast<ConfigSlot>(slot);
    return true;
}

bool hasExactFields(
    const ArduinoJson::JsonObject& object, const char* const* fields,
    std::size_t fieldCount) {
    if (object.size() != fieldCount) {
        return false;
    }
    for (std::size_t index = 0; index < fieldCount; ++index) {
        if (!object.containsKey(fields[index])) {
            return false;
        }
    }
    return true;
}

bool parseOutcome(const ArduinoJson::JsonObject& object, OtaResult& result) {
    static const char* const fields[] = {
        "status", "transferred", "hash", "version", "reason"};
    if (!hasExactFields(object, fields, sizeof(fields) / sizeof(fields[0]))) {
        return false;
    }

    OtaResult parsed;
    if (!parseString(object, "status", parsed.status) ||
        !parseInteger(object, "transferred", parsed.transferred) ||
        !parseString(object, "hash", parsed.hash) ||
        !parseString(object, "reason", parsed.reason)) {
        return false;
    }
    uint32_t version = 0;
    if (!parseInteger(object, "version", version)) {
        return false;
    }
    parsed.version = version;
    if (!isValidResult(parsed, true)) {
        return false;
    }
    result = parsed;
    return true;
}

bool setResultFields(ArduinoJson::JsonObject& object, const OtaResult& result) {
    object["status"] = result.status;
    object["transferred"] = result.transferred;
    object["hash"] = result.hash;
    if (result.version.has_value()) {
        object["version"] = *result.version;
    } else {
        object["version"] = ArduinoJson::RawJson("null");
    }
    object["reason"] = result.reason;
    return object.size() == 5;
}

std::string serializeResult(const OtaResult& result) {
    ArduinoJson::DynamicJsonBuffer buffer(512);
    ArduinoJson::JsonObject& object = buffer.createObject();
    if (!object.success() || !setResultFields(object, result)) {
        return std::string();
    }
    std::string serialized;
    object.printTo(serialized);
    return serialized;
}

std::string phaseName(UploadPhase phase) {
    switch (phase) {
    case UploadPhase::idle:
        return "idle";
    case UploadPhase::pending:
        return "pending";
    case UploadPhase::started:
        return "started";
    }
    return std::string();
}

bool setStatusFields(
    ArduinoJson::JsonObject& object, const UploadStatus& state) {
    object["format"] = static_cast<uint32_t>(1);
    object["sequence"] = state.sequence;
    object["activeSlot"] = static_cast<uint32_t>(state.activeSlot);
    object["activeVersion"] = state.activeVersion;
    object["phase"] = phaseName(state.phase);
    if (state.trialSlot.has_value()) {
        object["trialSlot"] = static_cast<uint32_t>(*state.trialSlot);
    } else {
        object["trialSlot"] = ArduinoJson::RawJson("null");
    }
    if (state.trialVersion.has_value()) {
        object["trialVersion"] = *state.trialVersion;
    } else {
        object["trialVersion"] = ArduinoJson::RawJson("null");
    }
    object["trialHash"] = state.trialHash;
    object["trialBytes"] = state.trialBytes;
    if (state.minimumAcceptedConfigVersion.has_value()) {
        object["minimumAcceptedConfigVersion"] =
            *state.minimumAcceptedConfigVersion;
    } else {
        object["minimumAcceptedConfigVersion"] = ArduinoJson::RawJson("null");
    }
    if (state.lastOutcome.has_value()) {
        ArduinoJson::JsonObject& outcome =
            object.createNestedObject("lastOutcome");
        if (!outcome.success() ||
            !setResultFields(outcome, *state.lastOutcome)) {
            return false;
        }
    } else {
        object["lastOutcome"] = ArduinoJson::RawJson("null");
    }
    return object.size() == 11;
}

std::string serializeStatusPayload(const UploadStatus& state) {
    ArduinoJson::DynamicJsonBuffer buffer(1024);
    ArduinoJson::JsonObject& object = buffer.createObject();
    if (!object.success() || !setStatusFields(object, state)) {
        return std::string();
    }
    std::string serialized;
    object.printTo(serialized);
    return serialized;
}

uint32_t crc32(const std::string& value) {
    uint32_t checksum = 0xffffffffu;
    for (char character : value) {
        checksum ^= static_cast<uint8_t>(character);
        for (uint8_t bit = 0; bit < 8; ++bit) {
            checksum = (checksum >> 1) ^ (0xedb88320u & (0u - (checksum & 1u)));
        }
    }
    return checksum ^ 0xffffffffu;
}

std::string checksumText(const std::string& value) {
    static const char digits[] = "0123456789abcdef";
    uint32_t checksum = crc32(value);
    std::string result(8, '0');
    for (std::size_t index = result.size(); index > 0; --index) {
        result[index - 1] = digits[checksum & 0x0fu];
        checksum >>= 4;
    }
    return result;
}

bool validChecksumText(const std::string& value) {
    if (value.size() != 8) {
        return false;
    }
    for (char character : value) {
        if (!isLowerHex(character)) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool decodeUploadStatus(const std::string& input, UploadStatus& output) {
    if (input.empty() || input.size() > maxMetadataBytes) {
        return false;
    }

    static const char* const fields[] = {
        "format",       "sequence",
        "activeSlot",   "activeVersion",
        "phase",        "trialSlot",
        "trialVersion", "trialHash",
        "trialBytes",   "minimumAcceptedConfigVersion",
        "lastOutcome",  "checksum"};
    ArduinoJson::DynamicJsonBuffer buffer(1024);
    const ArduinoJson::JsonVariant parsed =
        tools::parseFullJson(input, buffer, ARDUINOJSON_DEFAULT_NESTING_LIMIT);
    if (!parsed.success() || !parsed.is<ArduinoJson::JsonObject>()) {
        return false;
    }
    const ArduinoJson::JsonObject& object =
        parsed.as<ArduinoJson::JsonObject>();
    if (!hasExactFields(object, fields, sizeof(fields) / sizeof(fields[0]))) {
        return false;
    }

    UploadStatus decoded;
    uint32_t format = 0;
    std::string phase;
    std::string checksum;
    if (!parseInteger(object, "format", format) || format != 1 ||
        !parseInteger(object, "sequence", decoded.sequence) ||
        !parseSlot(object, "activeSlot", decoded.activeSlot) ||
        !parseInteger(object, "activeVersion", decoded.activeVersion) ||
        !parseString(object, "phase", phase) ||
        !parseOptionalSlot(object, "trialSlot", decoded.trialSlot) ||
        !parseOptionalInteger(object, "trialVersion", decoded.trialVersion) ||
        !parseString(object, "trialHash", decoded.trialHash) ||
        !parseInteger(object, "trialBytes", decoded.trialBytes) ||
        !parseOptionalInteger(
            object, "minimumAcceptedConfigVersion",
            decoded.minimumAcceptedConfigVersion) ||
        !parseString(object, "checksum", checksum) ||
        !validChecksumText(checksum)) {
        return false;
    }

    if (phase == "idle") {
        decoded.phase = UploadPhase::idle;
    } else if (phase == "pending") {
        decoded.phase = UploadPhase::pending;
    } else if (phase == "started") {
        decoded.phase = UploadPhase::started;
    } else {
        return false;
    }

    const auto outcome = object.get<ArduinoJson::JsonVariant>("lastOutcome");
    if (!isNull(outcome)) {
        if (!outcome.is<ArduinoJson::JsonObject>()) {
            return false;
        }
        OtaResult parsedOutcome;
        if (!parseOutcome(
                outcome.as<ArduinoJson::JsonObject>(), parsedOutcome)) {
            return false;
        }
        decoded.lastOutcome = parsedOutcome;
    }

    if (!isValidUploadStatus(decoded)) {
        return false;
    }
    const std::string payload = serializeStatusPayload(decoded);
    if (payload.empty() || checksumText(payload) != checksum) {
        return false;
    }
    output = decoded;
    return true;
}

std::string encodeUploadStatus(const UploadStatus& state) {
    if (!isValidUploadStatus(state)) {
        return std::string();
    }
    const std::string payload = serializeStatusPayload(state);
    if (payload.empty()) {
        return std::string();
    }
    std::string serialized = payload;
    serialized.pop_back();
    serialized += ",\"checksum\":\"";
    serialized += checksumText(payload);
    serialized += "\"}";
    if (serialized.size() > maxMetadataBytes) {
        return std::string();
    }
    return serialized;
}

bool advanceMinimum(UploadStatus& state, uint32_t acceptedVersion) {
    if (!state.minimumAcceptedConfigVersion.has_value() ||
        *state.minimumAcceptedConfigVersion == 0 ||
        acceptedVersion < *state.minimumAcceptedConfigVersion) {
        return false;
    }
    if (acceptedVersion == UINT32_MAX) {
        state.minimumAcceptedConfigVersion.reset();
    } else {
        state.minimumAcceptedConfigVersion = acceptedVersion + 1;
    }
    return true;
}

std::optional<uint32_t> initialMinimum(uint32_t activeVersion) {
    if (activeVersion == UINT32_MAX) {
        return std::nullopt;
    }
    return activeVersion + 1;
}

std::string encodeOtaResult(const OtaResult& result) {
    if (!isValidResult(result, false)) {
        return std::string();
    }
    return serializeResult(result);
}
