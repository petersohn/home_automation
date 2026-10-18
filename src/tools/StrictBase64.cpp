#include "StrictBase64.hpp"

#include "libb64/cdecode.h"

// Validate canonical base64 before passing it to the permissive libb64 decoder.

namespace tools {
namespace {

bool isStandardAlphabet(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '+' || c == '/';
}

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

    if (length % 4 != 0) {
        return false;
    }

    size_t padding = 0;
    if (input[length - 1] == '=') {
        padding = 1;
        if (input[length - 2] == '=') {
            padding = 2;
        }
    }

    for (size_t i = 0; i < length - padding; ++i) {
        if (!isStandardAlphabet(input[i])) {
            return false;
        }
    }

    // RFC 4648 requires all unused bits in the final padded group to be zero.
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

    // libb64 appends a NUL byte after decoded data.
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
