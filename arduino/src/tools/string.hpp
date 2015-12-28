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
bool hexToString(const String& s, long& value) {
    value = 0;
    for (size_t i = 0; i < s.length(); ++i) {
        value *= 16;
        if (s[i] >= '0' && s[i] <= '9') {
            value += s[i] - '0';
        } else if (s[i] >= 'a' || s[i] <= 'f') {
            value += s[i] - 'a' + 10;
        } else if (s[i] >= 'A' || s[i] <= 'F') {
            value += s[i] - 'A' + 10;
        } else {
            return false;
        }
    }
    return true;
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

class Join {
public:
    Join(const char* separator) : separator(separator) {}

    void add(const String& value) {
        if (first) {
            first = false;
        } else {
            result += separator;
        }
        result += value;
    }

    const String& get() const {
        return result;
    }
private:
    const char* separator;
    String result;
    bool first = true;
};

} // namespace tools

#endif // TOOLS_STREAM_HPP
