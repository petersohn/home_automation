#include <algorithm>
#include <cctype>

#include "string.hpp"

namespace tools {

std::string nextToken(
    const std::string& string, char separator, size_t& position) {
    while (position < string.length() && string[position] == separator) {
        ++position;
    }

    size_t startPos = position;
    while (position < string.length() && string[position] != separator) {
        ++position;
    }

    return string.substr(startPos, position - startPos);
}

std::string intToString(int value, unsigned radix) {
    constexpr const char* characters = "0123456789ABCDEF";

    if (radix > 16) {
        return "";
    }

    if (value == 0) {
        return "0";
    }

    std::string result;
    bool negative = value < 0;
    if (negative) {
        value *= -1;
    }
    while (value != 0) {
        result += characters[value % radix];
        value /= radix;
    }
    if (negative) {
        result += '-';
    }
    std::reverse(result.begin(), result.end());
    return result;
}

std::string floatToString(double value, int decimals) {
    int intpart = static_cast<int>(value);
    std::string result =
        intpart == 0 && value < 0 ? "-0" : intToString(intpart);
    value -= intpart;
    if (value == 0) {
        return result;
    }
    if (value < 0) {
        value *= -1;
    }
    result += '.';
    for (int i = 0; i < decimals; ++i) {
        if (value == 0) {
            break;
        }
        value *= 10;
        intpart = static_cast<int>(value);
        result += intpart + '0';
        value -= intpart;
    }
    return result;
}

bool getBoolValue(const char* input, bool& output, int length) {
    constexpr int maxLength = 5;
    char buf[maxLength + 1];
    if (length < 0) {
        length = strnlen(input, maxLength);
    } else {
        length = std::min(length, maxLength);
    }
    std::transform(
        input, input + length, buf, [](char c) { return std::tolower(c); });
    buf[length] = 0;

    if (strcmp(buf, "1") == 0 || strcmp(buf, "on") == 0 ||
        strcmp(buf, "true") == 0) {
        output = true;
        return true;
    }
    if (strcmp(buf, "0") == 0 || strcmp(buf, "off") == 0 ||
        strcmp(buf, "false") == 0) {
        output = false;
        return true;
    }
    return false;
}

}  // namespace tools
