#include "SignedEnvelope.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "../tools/StrictBase64.hpp"
#include "ArduinoJson.hpp"

// ArduinoJson 5 reads NUL-terminated strings without exposing parser position.
// Preserve input length to reject trailing data and embedded NULs.
struct SignedEnvelopeJsonInput {
    explicit SignedEnvelopeJsonInput(const std::string& value) : text(value) {}

    const std::string& text;
    std::size_t position = 0;
};

namespace ArduinoJson {
namespace Internals {

template <>
struct StringTraits<SignedEnvelopeJsonInput, void> {
    class Reader {
    public:
        explicit Reader(SignedEnvelopeJsonInput& input) : input(input) {}

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

        SignedEnvelopeJsonInput& input;
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

// Bound decoded allocation before base64 validation.
std::size_t decodedUpperBound(const std::string& encoded) {
    const std::size_t padding =
        encoded.size() >= 2 && encoded.compare(encoded.size() - 2, 2, "==") == 0
            ? 2
            : (!encoded.empty() && encoded.back() == '=' ? 1 : 0);
    const std::size_t groups =
        encoded.size() / 4 + (encoded.size() % 4 == 0 ? 0 : 1);
    return groups * 3 - padding;
}

bool hasOnlyTrailingWhitespace(const SignedEnvelopeJsonInput& input) {
    for (std::size_t i = input.position; i < input.text.size(); ++i) {
        const char value = input.text[i];
        if (value != ' ' && value != '\t' && value != '\n' && value != '\r') {
            return false;
        }
    }
    return true;
}

}  // namespace

EnvelopeError decodeSignedEnvelope(
    const std::string& command, SignedEnvelope& output) {
    output.data.clear();
    output.signature.fill(0);

    if (command.size() > maxCommandBytes) {
        return EnvelopeError::commandTooLarge;
    }
    if (command.find('\0') != std::string::npos) {
        return EnvelopeError::invalidJson;
    }

    SignedEnvelopeJsonInput input(command);
    ArduinoJson::DynamicJsonBuffer buffer{512};
    ArduinoJson::JsonVariant rootValue = buffer.parse(input, maxNestingDepth);
    if (!rootValue.success() || !hasOnlyTrailingWhitespace(input)) {
        return EnvelopeError::invalidJson;
    }
    if (!rootValue.is<ArduinoJson::JsonObject>()) {
        if (!rootValue.is<ArduinoJson::JsonArray>() &&
            !rootValue.is<const char*>() && !rootValue.is<bool>() &&
            !rootValue.is<ArduinoJson::Internals::JsonInteger>() &&
            !rootValue.is<ArduinoJson::Internals::JsonFloat>()) {
            return EnvelopeError::invalidJson;
        }
        return EnvelopeError::invalidFields;
    }

    ArduinoJson::JsonObject& root = rootValue.as<ArduinoJson::JsonObject>();
    if (root.size() != 2 || !root.containsKey("data") ||
        !root.containsKey("signature")) {
        return EnvelopeError::invalidFields;
    }
    if (!root.is<const char*>("data") || !root.is<const char*>("signature")) {
        return EnvelopeError::invalidFields;
    }

    const char* const dataText = root.get<const char*>("data");
    const char* const signatureText = root.get<const char*>("signature");
    if (dataText == nullptr || signatureText == nullptr) {
        return EnvelopeError::invalidFields;
    }

    const std::string dataEncoded(dataText);
    const std::string signatureEncoded(signatureText);
    if (decodedUpperBound(dataEncoded) > maxDataBytes) {
        return EnvelopeError::dataTooLarge;
    }

    std::vector<std::uint8_t> data;
    if (!tools::decodeCanonicalBase64(dataEncoded, maxDataBytes, data)) {
        return EnvelopeError::invalidBase64;
    }

    std::vector<std::uint8_t> signature;
    if (!tools::decodeCanonicalBase64(
            signatureEncoded, decodedUpperBound(signatureEncoded), signature)) {
        return EnvelopeError::invalidBase64;
    }
    if (signature.size() != signatureBytes) {
        return EnvelopeError::invalidSignatureLength;
    }

    output.data = std::move(data);
    for (std::size_t i = 0; i < signatureBytes; ++i) {
        output.signature[i] = signature[i];
    }
    return EnvelopeError::none;
}
