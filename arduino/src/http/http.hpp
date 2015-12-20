#ifndef HTTP_HTTP_HPP
#define HTTP_HTTP_HPP

#include "tools/string.hpp"

namespace http {


struct Header {
    const char* name;
    const char* value;
};

template <typename Stream>
void sendRequest(Stream& stream, const char* method, const char* path) {
    Serial.print("---> ");
    Serial.print(method);
    Serial.print(' ');
    Serial.print(path);
    Serial.println(" HTTP/1.1");

    stream.print(method);
    stream.print(' ');
    stream.print(path);
    stream.println(" HTTP/1.1");
}

template <typename Stream>
void sendResponse(Stream& stream, int statusCode, const char* description) {
    Serial.print("---> HTTP/1.1 ");
    Serial.print(statusCode);
    Serial.print(' ');
    Serial.println(description);

    stream.print("HTTP/1.1 ");
    stream.print(statusCode);
    stream.print(' ');
    stream.println(description);
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
void sendHeaders(Stream& stream, const Header* headers) {
    if (headers) {
        while (headers->name != nullptr) {
            sendHeader(stream, headers->name, headers->value);
            ++headers;
        }
    }
}

template <typename Stream>
void sendHeadersEnd(Stream& stream) {
    stream.println();
}

template <typename Stream>
void sendError(Stream& stream, int statusCode, const char* description,
        const Header* additionalHeaders, bool sendBody) {
    String content = "{ \"statusCode\": " + String(statusCode) +
            ", \"description\": \"" + description + "\" }";
    sendResponse(stream, statusCode, description);
    sendHeader(stream, "Content-Length", content.length());
    sendHeaders(stream, additionalHeaders);
    sendHeadersEnd(stream);
    if (sendBody) {
        stream.print(content);
    }
}

template <typename Stream>
void sendError(Stream& stream, int statusCode, const String& description) {
    sendError(stream, statusCode, description.c_str());
}

template <typename Stream>
bool receiveResponse(Stream& stream, int& statusCode) {
    String line = tools::readLine(stream);
    Serial.print("<--- ");
    Serial.println(line);

    if (line.startsWith("HTTP/")) {
        int index = line.indexOf(' ');
        statusCode = line.substring(index + 1, index + 4).toInt();
        return statusCode >= 100;
    }
    return false;
}

template <typename Stream>
bool receiveRequest(Stream& stream, String& method, String& path) {
    String line = tools::readLine(stream);
    Serial.print("<--- ");
    Serial.println(line);
    size_t position = 0;

    method = tools::nextToken(line, ' ', position);
    if (method.length() == 0) {
        return false;
    }

    path = tools::nextToken(line, ' ', position);
    if (path.length() == 0) {
        return false;
    }

    String httpString = tools::nextToken(line, ' ', position);
    return httpString.startsWith("HTTP/");
}

template <typename Stream>
bool receiveHeader(Stream& stream, String& name, String& value) {
    String line = tools::readLine(stream);
    if (line.length() == 0) {
        name = "";
        return true;
    }

    int index = line.indexOf(": ");
    if (index == -1) {
        return false;
    }
    name = line.substring(0, index);
    value = line.substring(index + 2);
    return true;
}

} // namespace http

#endif // HTTP_HTTP_HPP
