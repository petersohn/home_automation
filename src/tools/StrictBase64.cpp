#include "StrictBase64.hpp"

#include "libb64/cdecode.h"

// Strict canonical base64 decoding. The decoding itself is not implemented
// here: after strict validation succeeds, decoding is delegated to the
// permissive libb64 decoder bundled with the ESP8266 core (and vendored
// verbatim for the native tests). The validation enforces the RFC 4648
// canonical form that the library decoder does not check: the standard
// alphabet only, padding exactly at the end, and zero unused bits.

namespace tools {
namespace {

// True only for the standard RFC 4648 base64 alphabet. Any other byte
// (whitespace, URL-safe characters, controls, high-bit bytes) is rejected.
bool isStandardAlphabet(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '+' || c == '/';
}

// 6-bit value of an alphabet character. Must only be called with characters
// accepted by isStandardAlphabet (the '+' and '/' cases are unreachable
// otherwise).
uint8_t decodeChar(char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<uint8_t>(c - 'A');
    }
    if (c >= 'a' && c <= 'z') {
        return static_cast<uint8_t>(c - 'a' + 26);
    }
    if (c >= '0' && c <= '9') {
        return static_cast<uint8_t>(c - '0' + 52);
    }
    if (c == '+') {
        return 62;
    }
    return 63;  // '/'
}

}  // namespace

bool decodeCanonicalBase64(
    const std::string& input, size_t maxOutput, std::vector<uint8_t>& output) {
    output.clear();

    const size_t length = input.size();
    if (length == 0) {
        return true;
    }

    // The canonical length is a nonzero multiple of 4, so every remaining
    // partial group fails, including one or three padding characters.
    if (length % 4 != 0) {
        return false;
    }

    // '=' is only legal as the final one or two padding characters; the scan
    // below rejects it anywhere else.
    size_t padding = 0;
    if (input[length - 1] == '=') {
        padding = 1;
        if (input[length - 2] == '=') {
            padding = 2;
        }
    }

    // Every non-padding character must be from the standard alphabet. This
    // rejects misplaced padding together with any other invalid byte.
    for (size_t i = 0; i < length - padding; ++i) {
        if (!isStandardAlphabet(input[i])) {
            return false;
        }
    }

    // Canonical form: the unused bits of the final partial group are zero.
    // An XY== group keeps only the top two bits of its second data
    // character; an XYZ= group keeps the top four.
    if (padding == 2 && (decodeChar(input[length - 3]) & 0x0F) != 0) {
        return false;
    }
    if (padding == 1 && (decodeChar(input[length - 2]) & 0x03) != 0) {
        return false;
    }

    const size_t expectedSize = (length / 4) * 3 - padding;
    if (expectedSize > maxOutput) {
        return false;
    }

    // The library decoder writes a NUL terminator after the decoded bytes,
    // so the buffer must hold one extra byte while decoding.
    output.resize(expectedSize + 1);
    const int decodedSize = base64_decode_chars(
        input.c_str(), static_cast<int>(length),
        reinterpret_cast<char*>(output.data()));
    if (static_cast<size_t>(decodedSize) != expectedSize) {
        output.clear();
        return false;
    }
    output.resize(expectedSize);
    return true;
}

}  // namespace tools
