#ifndef TOOLS_STREAM_HPP
#define TOOLS_STREAM_HPP

#include <Arduino.h>

namespace tools {

template <typename Stream>
String readLine(Stream& stream) {
    String result;
    while (true) {
        if (!stream.connected()) {
            return result;
        }

        char ch = stream.read();
        if (ch == '\r' || ch == '\xff') {
            continue;
        }

        if (ch == '\n') {
            return result;
        }

        result += ch;
    }
}

inline
String nextToken(const String& string, char separator, size_t& position) {
    while (position < string.length() && string[position] == separator) {
        ++position;
    }

    size_t startPos = position;
    while (position < string.length() && string[position] != separator) {
        ++position;
    }

    return string.substring(startPos, position);
}

} // namespace tools

#endif // TOOLS_STREAM_HPP
