#include "StreamCommand.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../tools/Hex.hpp"
#include "../tools/JsonParsing.hpp"
#include "../tools/StrictBase64.hpp"
#include "ArduinoJson.hpp"

namespace {

constexpr std::size_t maxCommandBytes = 512;
constexpr std::size_t maxDataBytes = 256;
constexpr std::size_t signatureBytes = 64;
constexpr std::uint8_t maxNestingDepth = 8;

}  // namespace

StreamCommandError decodeStreamCommand(
    const std::string& text, StreamCommand& output) {
    output = StreamCommand{};
    if (text.size() > maxCommandBytes) {
        return StreamCommandError::commandTooLarge;
    }
    ArduinoJson::DynamicJsonBuffer buffer{512};
    ArduinoJson::JsonVariant rootValue =
        tools::parseFullJson(text, buffer, maxNestingDepth);
    if (!rootValue.success()) {
        return StreamCommandError::invalidJson;
    }
    if (!rootValue.is<ArduinoJson::JsonObject>()) {
        return StreamCommandError::invalidFields;
    }

    ArduinoJson::JsonObject& root = rootValue.as<ArduinoJson::JsonObject>();
    if (!root.containsKey("type") || !root.is<const char*>("type")) {
        return StreamCommandError::invalidFields;
    }
    const char* const typeText = root.get<const char*>("type");
    if (typeText == nullptr) {
        return StreamCommandError::invalidFields;
    }

    StreamCommand decoded;
    std::size_t expectedFields = 0;
    const char* const hashTextKey = "hash";
    if (std::string(typeText) == "begin") {
        decoded.type = StreamCommandType::begin;
        expectedFields = 3;
        if (!root.containsKey("size")) {
            return StreamCommandError::invalidFields;
        }
    } else if (std::string(typeText) == "chunk") {
        decoded.type = StreamCommandType::chunk;
        expectedFields = 4;
        if (!root.containsKey("offset") || !root.containsKey("data")) {
            return StreamCommandError::invalidFields;
        }
    } else if (std::string(typeText) == "finish") {
        decoded.type = StreamCommandType::finish;
        expectedFields = 3;
        if (!root.containsKey("signature")) {
            return StreamCommandError::invalidFields;
        }
    } else {
        return StreamCommandError::invalidFields;
    }

    if (root.size() != expectedFields || !root.containsKey(hashTextKey)) {
        return StreamCommandError::invalidFields;
    }
    if ((decoded.type == StreamCommandType::begin &&
         !root.is<ArduinoJson::Internals::JsonInteger>("size")) ||
        (decoded.type == StreamCommandType::chunk &&
         (!root.is<ArduinoJson::Internals::JsonInteger>("offset") ||
          !root.is<const char*>("data"))) ||
        (decoded.type == StreamCommandType::finish &&
         !root.is<const char*>("signature")) ||
        !root.is<const char*>(hashTextKey)) {
        return StreamCommandError::invalidFields;
    }

    const char* const hashText = root.get<const char*>(hashTextKey);
    if (hashText == nullptr) {
        return StreamCommandError::invalidFields;
    }
    if (!tools::decodeLowercaseHex(hashText, decoded.hash)) {
        return StreamCommandError::invalidHash;
    }

    if (decoded.type == StreamCommandType::begin) {
        if (!tools::parseUint32(
                root.get<ArduinoJson::JsonVariant>("size"), decoded.size)) {
            return StreamCommandError::invalidFields;
        }
    } else if (decoded.type == StreamCommandType::chunk) {
        if (!tools::parseUint32(
                root.get<ArduinoJson::JsonVariant>("offset"), decoded.offset)) {
            return StreamCommandError::invalidFields;
        }
        const char* const dataText = root.get<const char*>("data");
        if (dataText == nullptr) {
            return StreamCommandError::invalidFields;
        }
        const std::string encoded(dataText);
        if (tools::decodedUpperBound(encoded) > maxDataBytes) {
            return StreamCommandError::dataTooLarge;
        }
        std::vector<uint8_t> data;
        if (!tools::decodeCanonicalBase64(encoded, maxDataBytes, data)) {
            return StreamCommandError::invalidBase64;
        }
        if (data.empty()) {
            return StreamCommandError::invalidFields;
        }
        decoded.dataSize = data.size();
        for (std::size_t i = 0; i < data.size(); ++i) {
            decoded.data[i] = data[i];
        }
    } else {
        const char* const signatureText = root.get<const char*>("signature");
        if (signatureText == nullptr) {
            return StreamCommandError::invalidFields;
        }
        const std::string encoded(signatureText);
        std::vector<uint8_t> signature;
        if (!tools::decodeCanonicalBase64(
                encoded, tools::decodedUpperBound(encoded), signature)) {
            return StreamCommandError::invalidBase64;
        }
        if (signature.size() != signatureBytes) {
            return StreamCommandError::invalidSignatureLength;
        }
        for (std::size_t i = 0; i < signature.size(); ++i) {
            decoded.signature[i] = signature[i];
        }
    }

    output = decoded;
    return StreamCommandError::none;
}

std::string streamHashText(const std::array<uint8_t, 32>& hash) {
    static const char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(64);
    for (uint8_t value : hash) {
        text.push_back(digits[value >> 4]);
        text.push_back(digits[value & 0x0F]);
    }
    return text;
}

StreamChunkPosition classifyStreamChunk(
    uint32_t total, uint32_t received, uint32_t offset, size_t length) {
    if (length == 0 || length > maxDataBytes || received > total ||
        offset > total || length > static_cast<size_t>(total - offset)) {
        return StreamChunkPosition::invalid;
    }
    if (offset == received) {
        return StreamChunkPosition::next;
    }
    if (offset < received && length <= static_cast<size_t>(received - offset)) {
        return StreamChunkPosition::duplicate;
    }
    return StreamChunkPosition::invalid;
}
