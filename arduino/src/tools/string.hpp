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
        if (ch == '\r' || ch == 255) {
            continue;
        }

        if (ch == '\n') {
            return result;
        }

        result += ch;
    }
}

} // namespace tools

#endif // TOOLS_STREAM_HPP
