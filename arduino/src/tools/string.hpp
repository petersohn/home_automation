#ifndef TOOLS_STREAM_HPP
#define TOOLS_STREAM_HPP

#include <Arduino.h>

namespace tools {

template <typename Stream>
String readLine(Stream& stream) {
    String result;
    while (true) {
        while (stream.connected() && !stream.available()) {
            yield();
        }
        if (!stream.connected()) {
            return result;
        }

        char ch = stream.read();
        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            return result;
        }

        result += ch;
    }
}

template <typename Stream>
String readBuffer(Stream& stream, int size) {
    String result;
    while (size > 0) {
        while (stream.connected() && !stream.available()) {
            yield();
        }
        if (!stream.connected()) {
            return result;
        }

        unsigned char buffer[1000];
        int amount = stream.read(buffer, (size > 999 ? 999 : size));
        buffer[amount] = 0;
        result += reinterpret_cast<char*>(buffer);
        size -= amount;
    }
    return result;
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
