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
