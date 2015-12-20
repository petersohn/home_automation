#ifndef HTTP_HTTP_HPP
#define HTTP_HTTP_HPP

#include "tools/string.hpp"

namespace http {

template <typename Stream>
void sendRequest(Stream& stream, const char* command, const char* path) {
    stream.print(command);
    stream.print(' ');
    stream.print(path);
    stream.println(" HTTP/1.1");
}

template <typename Stream>
void sendHeader(Stream& stream, const char* name, const char* value) {
    stream.print(name);
    stream.print(": ");
    stream.println(value);
}

template <typename Stream>
void sendHeader(Stream& stream, const char* name, const String& value) {
    sendHeader(stream, name, value.c_str());
}

template <typename Stream, typename Value>
void sendHeader(Stream& stream, const char* name, const Value& value) {
    sendHeader(stream, name, String(value).c_str());
}

template <typename Stream>
void sendHeadersEnd(Stream& stream) {
    stream.println();
}

template <typename Stream>
bool receiveAnswerStatus(Stream& stream, int& statusCode) {
    String line = tools::readLine(stream);

    if (line.startsWith("HTTP")) {
        int index = line.indexOf(' ');
        statusCode = line.substring(index + 1, index + 4).toInt();
        return statusCode >= 100;
    }
    return false;
}

} // namespace http


#endif // HTTP_HTTP_HPP
