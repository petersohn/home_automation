#include "SignedEnvelope.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "../tools/StrictBase64.hpp"

// Strict decoder for the signed envelope. The command size limit is checked
// before any parsing. The JSON document is validated with a bounded
// syntax-aware scanner: it validates the entire document, unescapes strings
// (including \uXXXX escapes) so that key names take their unescaped meaning,
// and captures only the data/signature string fields of the root object.
// Escape-aliased duplicates and unknown fields are detected before any map
// collapse could hide them. The scanner validates nested values for syntax
// only and bounds nesting depth; it does not build a general-purpose JSON
// representation.

namespace {

constexpr size_t maxCommandBytes = 512;
constexpr size_t maxDataBytes = 256;
constexpr size_t signatureBytes = 64;
constexpr int maxNestingDepth = 8;

// Number of '=' padding characters at the end of an encoded string, counting
// at most the two canonical trailing ones.
size_t trailingPadding(const std::string& encoded) {
    if (encoded.empty() || encoded[encoded.size() - 1] != '=') {
        return 0;
    }
    if (encoded.size() >= 2 && encoded[encoded.size() - 2] == '=') {
        return 2;
    }
    return 1;
}

// Upper bound of the decoded size for a base64 string, assuming canonical
// padding. Oversized encoded content is rejected before any decoded buffer
// is allocated.
size_t decodedUpperBound(const std::string& encoded) {
    const size_t groups =
        encoded.size() / 4 + (encoded.size() % 4 != 0 ? 1 : 0);
    return groups * 3 - trailingPadding(encoded);
}

bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

// Bounded syntax-aware JSON scanner for the envelope format.
class JsonScanner {
public:
    explicit JsonScanner(const std::string& input) : input(input) {}

    std::string dataValue;
    std::string signatureValue;
    bool hasData = false;
    bool hasSignature = false;

    // Validates the whole document. Returns none when the root is an object
    // holding only data/signature string fields, invalidJson for malformed
    // syntax (including trailing data), and invalidFields for a syntactically
    // valid document with the wrong envelope shape. On success dataValue and
    // signatureValue hold the unescaped field contents.
    EnvelopeError parseDocument() {
        this->skipWhitespace();
        if (!this->parseValue(true)) {
            return EnvelopeError::invalidJson;
        }
        this->skipWhitespace();
        if (this->pos != this->input.size()) {
            return EnvelopeError::invalidJson;
        }
        return this->shape;
    }

private:
    const std::string& input;
    size_t pos = 0;
    int depth = 0;
    EnvelopeError shape = EnvelopeError::none;

    char peek() const { return this->input[this->pos]; }

    bool atEnd() const { return this->pos == this->input.size(); }

    void advance() { ++this->pos; }

    void skipWhitespace() {
        while (!this->atEnd()) {
            const char c = this->peek();
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                break;
            }
            this->advance();
        }
    }

    // Validates a JSON value. capture is true only for the document root: it
    // extracts the envelope fields from the root object and marks every
    // other root value as a shape error.
    bool parseValue(bool capture) {
        if (this->atEnd()) {
            return false;
        }
        switch (this->peek()) {
        case '{':
            return this->parseObject(capture);
        case '[':
            if (capture) {
                this->shape = EnvelopeError::invalidFields;
            }
            return this->parseArray();
        case '"': {
            if (capture) {
                this->shape = EnvelopeError::invalidFields;
            }
            std::string discard;
            return this->parseString(discard);
        }
        case 't':
            return this->parseKeyword(capture, "true");
        case 'f':
            return this->parseKeyword(capture, "false");
        case 'n':
            return this->parseKeyword(capture, "null");
        default:
            if (capture) {
                this->shape = EnvelopeError::invalidFields;
            }
            return this->parseNumber();
        }
    }

    bool parseObject(bool capture) {
        ++this->pos;
        if (++this->depth > maxNestingDepth) {
            return false;
        }
        this->skipWhitespace();
        if (!this->atEnd() && this->peek() == '}') {
            this->advance();
            --this->depth;
            return true;
        }
        while (true) {
            this->skipWhitespace();
            if (this->atEnd() || this->peek() != '"') {
                return false;
            }
            std::string key;
            if (!this->parseString(key)) {
                return false;
            }
            this->skipWhitespace();
            if (!this->expect(':')) {
                return false;
            }
            this->skipWhitespace();
            if (capture && key == "data") {
                if (!this->captureString(this->dataValue, this->hasData)) {
                    return false;
                }
            } else if (capture && key == "signature") {
                if (!this->captureString(
                        this->signatureValue, this->hasSignature)) {
                    return false;
                }
            } else {
                if (!this->parseValue(false)) {
                    return false;
                }
                if (capture) {
                    this->shape = EnvelopeError::invalidFields;
                }
            }
            this->skipWhitespace();
            if (this->atEnd()) {
                return false;
            }
            if (this->peek() == ',') {
                this->advance();
                continue;
            }
            if (this->peek() == '}') {
                this->advance();
                --this->depth;
                return true;
            }
            return false;
        }
    }

    // Captures one envelope field at the root object. Its value must be a
    // string; a repeated key is a duplicate field. Any other shape error is
    // recorded but the value still has to be syntactically valid JSON.
    bool captureString(std::string& value, bool& present) {
        if (present) {
            this->shape = EnvelopeError::invalidFields;
        }
        present = true;
        if (this->peek() == '"') {
            value.clear();
            return this->parseString(value);
        }
        if (!this->parseValue(false)) {
            return false;
        }
        this->shape = EnvelopeError::invalidFields;
        return true;
    }

    bool parseArray() {
        ++this->pos;
        if (++this->depth > maxNestingDepth) {
            return false;
        }
        this->skipWhitespace();
        if (!this->atEnd() && this->peek() == ']') {
            this->advance();
            --this->depth;
            return true;
        }
        while (true) {
            if (!this->parseValue(false)) {
                return false;
            }
            this->skipWhitespace();
            if (this->atEnd()) {
                return false;
            }
            if (this->peek() == ',') {
                this->advance();
                continue;
            }
            if (this->peek() == ']') {
                this->advance();
                --this->depth;
                return true;
            }
            return false;
        }
    }

    // Parses a string, appending its unescaped bytes to out.
    bool parseString(std::string& out) {
        ++this->pos;
        while (true) {
            if (this->atEnd()) {
                return false;
            }
            const char c = this->peek();
            if (c == '"') {
                this->advance();
                return true;
            }
            if (c == '\\') {
                if (!this->parseEscape(out)) {
                    return false;
                }
                continue;
            }
            // Raw control characters are not allowed inside strings.
            if (static_cast<unsigned char>(c) < 0x20) {
                return false;
            }
            out.push_back(c);
            this->advance();
        }
    }

    bool parseEscape(std::string& out) {
        this->advance();
        if (this->atEnd()) {
            return false;
        }
        const char c = this->peek();
        this->advance();
        switch (c) {
        case '"':
            out.push_back('"');
            return true;
        case '\\':
            out.push_back('\\');
            return true;
        case '/':
            out.push_back('/');
            return true;
        case 'b':
            out.push_back('\b');
            return true;
        case 'f':
            out.push_back('\f');
            return true;
        case 'n':
            out.push_back('\n');
            return true;
        case 'r':
            out.push_back('\r');
            return true;
        case 't':
            out.push_back('\t');
            return true;
        case 'u':
            return this->parseUnicodeEscape(out);
        default:
            return false;
        }
    }

    // Parses a \uXXXX escape, combining a following low surrogate into one
    // code point. A truncated or non-hex escape is invalid JSON. Lone
    // surrogates are encoded as three-byte values; they can never produce
    // valid base64 content.
    bool parseUnicodeEscape(std::string& out) {
        uint32_t codePoint = 0;
        if (!this->parseHex4(codePoint)) {
            return false;
        }
        if (codePoint >= 0xD800 && codePoint <= 0xDBFF &&
            this->pos + 2 <= this->input.size() &&
            this->input[this->pos] == '\\' &&
            this->input[this->pos + 1] == 'u') {
            const size_t restart = this->pos;
            this->pos += 2;
            uint32_t low = 0;
            if (!this->parseHex4(low)) {
                return false;
            }
            if (low >= 0xDC00 && low <= 0xDFFF) {
                codePoint =
                    0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
            } else {
                this->pos = restart;
            }
        }
        this->appendUtf8(out, codePoint);
        return true;
    }

    bool parseHex4(uint32_t& value) {
        if (this->pos + 4 > this->input.size()) {
            return false;
        }
        uint32_t result = 0;
        for (int i = 0; i < 4; ++i) {
            const unsigned char c =
                static_cast<unsigned char>(this->input[this->pos + i]);
            result <<= 4;
            if (c >= '0' && c <= '9') {
                result |= c - '0';
            } else if (c >= 'a' && c <= 'f') {
                result |= c - 'a' + 10;
            } else if (c >= 'A' && c <= 'F') {
                result |= c - 'A' + 10;
            } else {
                return false;
            }
        }
        this->pos += 4;
        value = result;
        return true;
    }

    void appendUtf8(std::string& out, uint32_t codePoint) {
        if (codePoint < 0x80) {
            out.push_back(static_cast<char>(codePoint));
        } else if (codePoint < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else if (codePoint < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }

    bool parseNumber() {
        if (!this->atEnd() && this->peek() == '-') {
            this->advance();
        }
        if (this->atEnd() || !isDigit(this->peek())) {
            return false;
        }
        if (this->peek() == '0') {
            this->advance();
        } else {
            while (!this->atEnd() && isDigit(this->peek())) {
                this->advance();
            }
        }
        if (!this->atEnd() && this->peek() == '.') {
            this->advance();
            if (this->atEnd() || !isDigit(this->peek())) {
                return false;
            }
            while (!this->atEnd() && isDigit(this->peek())) {
                this->advance();
            }
        }
        if (!this->atEnd() && (this->peek() == 'e' || this->peek() == 'E')) {
            this->advance();
            if (!this->atEnd() &&
                (this->peek() == '+' || this->peek() == '-')) {
                this->advance();
            }
            if (this->atEnd() || !isDigit(this->peek())) {
                return false;
            }
            while (!this->atEnd() && isDigit(this->peek())) {
                this->advance();
            }
        }
        return true;
    }

    // Validates the true/false/null keywords.
    bool parseKeyword(bool capture, const char* literal) {
        if (capture) {
            this->shape = EnvelopeError::invalidFields;
        }
        for (const char* c = literal; *c != '\0'; ++c) {
            if (this->atEnd() || this->peek() != *c) {
                return false;
            }
            this->advance();
        }
        return true;
    }

    bool expect(char c) {
        if (this->atEnd() || this->peek() != c) {
            return false;
        }
        this->advance();
        return true;
    }
};

}  // namespace

EnvelopeError decodeSignedEnvelope(
    const std::string& command, SignedEnvelope& output) {
    output.data.clear();
    output.signature.fill(0);

    // Commands are rejected before parsing or copying into any storage.
    if (command.size() > maxCommandBytes) {
        return EnvelopeError::commandTooLarge;
    }

    JsonScanner scanner(command);
    const EnvelopeError structureError = scanner.parseDocument();
    if (structureError != EnvelopeError::none) {
        return structureError;
    }
    if (!scanner.hasData || !scanner.hasSignature) {
        return EnvelopeError::invalidFields;
    }

    // The data limit is enforced before any decoded buffer is allocated.
    if (decodedUpperBound(scanner.dataValue) > maxDataBytes) {
        return EnvelopeError::dataTooLarge;
    }

    std::vector<uint8_t> data;
    if (!tools::decodeCanonicalBase64(scanner.dataValue, maxDataBytes, data)) {
        return EnvelopeError::invalidBase64;
    }

    std::vector<uint8_t> signature;
    if (!tools::decodeCanonicalBase64(
            scanner.signatureValue, decodedUpperBound(scanner.signatureValue),
            signature)) {
        return EnvelopeError::invalidBase64;
    }
    if (signature.size() != signatureBytes) {
        return EnvelopeError::invalidSignatureLength;
    }

    output.data = std::move(data);
    for (size_t i = 0; i < signatureBytes; ++i) {
        output.signature[i] = signature[i];
    }
    return EnvelopeError::none;
}
