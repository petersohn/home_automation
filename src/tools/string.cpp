#include "string.hpp"

#include <algorithm>
#include <cctype>

namespace tools {

std::string nextToken(const std::string& string, char separator,
        size_t& position) {
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
    std::string result = intToString(intpart);
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

bool getBoolValue(std::string input, bool& output) {
    std::transform(input.begin(), input.end(), input.begin(),
            [](char c) { return std::tolower(c); });
    if (input == "1" || input == "on" || input == "true") {
        output = true;
        return true;
    }
    if (input == "0" || input == "off" || input == "false") {
        output = false;
        return true;
    }
    return false;
}

} // namespace tools
