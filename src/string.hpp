#ifndef TOOLS_STREAM_HPP
#define TOOLS_STREAM_HPP

#include <Arduino.h>

#include <cstring>
#include <memory>

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
bool hexToString(const char* s, size_t length, long& value) {
    value = 0;
    for (size_t i = 0; i < length; ++i) {
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
bool hexToString(const String& s, long& value) {
    return hexToString(s.c_str(), s.length(), value);
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

// Seriously, why doesn't String support this?
inline
String toString(const char* characters, std::size_t length) {
    std::unique_ptr<char[]> buffer{new char[length + 1]};
    std::memcpy(buffer.get(), characters, length);
    buffer[length] = 0;
    return String{buffer.get()};
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

namespace detail {

template<typename Range>
void addValue(String& result, const String& reference, const Range& elements) {
    int value = reference.toInt();
    // DEBUGLN("reference = " + reference);
    // DEBUGLN("value = " + String(value));
    if (value > 0 && value <= elements.size()) {
        result += elements[value - 1];
    }
}

} // namespace detail

template<typename Range>
String substitute(const String& valueTemplate, const Range& elements) {
    String result;
    String reference;
    bool inReference = false;
    for (std::size_t i = 0; i < valueTemplate.length(); ++i) {
        if (inReference) {
            if (valueTemplate[i] >= '0' && valueTemplate[i] <= '9') {
                reference += valueTemplate[i];
            } else {
                inReference = false;
                if (reference.length() == 0) {
                    result += valueTemplate[i];
                    reference = "";
                    continue;
                }
                detail::addValue(result, reference, elements);
                reference = "";
            }
        }
        if (!inReference) {
            if (valueTemplate[i] == '%') {
                inReference = true;
            } else {
                result += valueTemplate[i];
            }
        }
    }
    detail::addValue(result, reference, elements);
    return result;
}

inline
bool getBoolValue(const String& input, bool& output, bool allowToggle = true) {
    if (input == "1" || input.equalsIgnoreCase("on")
            || input.equalsIgnoreCase("true")) {
        output = true;
        return true;
    }
    if (input == "0" || input.equalsIgnoreCase("off")
            || input.equalsIgnoreCase("false")) {
        output = false;
        return true;
    }
    if (allowToggle && input.equalsIgnoreCase("toggle")) {
        output = !output;
        return true;
    }
    return false;
}

} // namespace tools

#endif // TOOLS_STREAM_HPP
