#include "StreamCommand.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../tools/StrictBase64.hpp"
#include "ArduinoJson.hpp"

struct StreamCommandJsonInput {
    explicit StreamCommandJsonInput(const std::string& value) : text(value) {}

    const std::string& text;
    std::size_t position = 0;
};

namespace ArduinoJson {
namespace Internals {

template <>
struct StringTraits<StreamCommandJsonInput, void> {
    class Reader {
    public:
        explicit Reader(StreamCommandJsonInput& input) : input(input) {}

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

        StreamCommandJsonInput& input;
    };

    static const bool has_append = false;
    static const bool has_equals = false;
};

}  // namespace Internals
}  // namespace ArduinoJson

namespace {

constexpr std::size_t maxCommandBytes = 512;
constexpr std::size_t maxDataBytes = 256;
constexpr std::size_t signatureBytes = 64;
constexpr std::uint8_t maxNestingDepth = 8;

std::size_t decodedUpperBound(const std::string& encoded) {
    const std::size_t padding =
        encoded.size() >= 2 && encoded.compare(encoded.size() - 2, 2, "==") == 0
            ? 2
            : (!encoded.empty() && encoded.back() == '=' ? 1 : 0);
    const std::size_t groups =
        encoded.size() / 4 + (encoded.size() % 4 == 0 ? 0 : 1);
    return groups * 3 - padding;
}

bool hasOnlyTrailingWhitespace(const StreamCommandJsonInput& input) {
    for (std::size_t i = input.position; i < input.text.size(); ++i) {
        const char value = input.text[i];
        if (value != ' ' && value != '\t' && value != '\n' && value != '\r') {
            return false;
        }
    }
    return true;
}

int hexValue(char value) {
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
}

bool parseHash(const std::string& text, std::array<uint8_t, 32>& hash) {
    if (text.size() != 64) {
        return false;
    }
    for (std::size_t i = 0; i < hash.size(); ++i) {
        const char high = text[2 * i];
        const char low = text[2 * i + 1];
        if (!((high >= '0' && high <= '9') || (high >= 'a' && high <= 'f')) ||
            !((low >= '0' && low <= '9') || (low >= 'a' && low <= 'f'))) {
            return false;
        }
        hash[i] = static_cast<uint8_t>((hexValue(high) << 4) | hexValue(low));
    }
    return true;
}

bool parseCount(const ArduinoJson::JsonVariant& value, uint32_t& result) {
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
    result = static_cast<uint32_t>(parsed);
    return true;
}

}  // namespace

StreamCommandError decodeStreamCommand(
    const std::string& text, StreamCommand& output) {
    output = StreamCommand{};
    if (text.size() > maxCommandBytes) {
        return StreamCommandError::commandTooLarge;
    }
    if (text.find('\0') != std::string::npos) {
        return StreamCommandError::invalidJson;
    }

    StreamCommandJsonInput input(text);
    ArduinoJson::DynamicJsonBuffer buffer{512};
    ArduinoJson::JsonVariant rootValue = buffer.parse(input, maxNestingDepth);
    if (!rootValue.success() || !hasOnlyTrailingWhitespace(input)) {
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
    if (!parseHash(hashText, decoded.hash)) {
        return StreamCommandError::invalidHash;
    }

    if (decoded.type == StreamCommandType::begin) {
        if (!parseCount(
                root.get<ArduinoJson::JsonVariant>("size"), decoded.size)) {
            return StreamCommandError::invalidFields;
        }
    } else if (decoded.type == StreamCommandType::chunk) {
        if (!parseCount(
                root.get<ArduinoJson::JsonVariant>("offset"), decoded.offset)) {
            return StreamCommandError::invalidFields;
        }
        const char* const dataText = root.get<const char*>("data");
        if (dataText == nullptr) {
            return StreamCommandError::invalidFields;
        }
        const std::string encoded(dataText);
        if (decodedUpperBound(encoded) > maxDataBytes) {
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
                encoded, decodedUpperBound(encoded), signature)) {
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
