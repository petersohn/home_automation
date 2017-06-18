#ifndef HTTP_HTTP_HPP
#define HTTP_HTTP_HPP

#include "debug.hpp"

#include "string.hpp"

namespace http {


template <typename Stream>
void sendRequest(Stream& stream, const String& method, const String& path) {
    String s = method + " " + path + " HTTP/1.1\r\n";
    DEBUG("--> ");
    DEBUG(s);
    stream.print(s);
}

template <typename Stream>
void sendResponse(Stream& stream, int statusCode, const char* description) {
    String s = "HTTP/1.1 " + String(statusCode) + " " + description + "\r\n";
    DEBUG("--> ");
    DEBUG(s);
    stream.print(s);
}

template <typename Stream>
void sendHeader(Stream& stream, const String& name, const String& value) {
    stream.print(name + ": " + value + "\r\n");
}

template <typename Stream>
void sendHeadersEnd(Stream& stream) {
    stream.println();
}

inline
String createErrorContent(int statusCode, const char* description,
        const String& details) {
    return "{ \"statusCode\": " + String(statusCode) +
            ", \"description\": \"" + String(description) +
            "\", \"details\": \"" + details + "\" }";
}

template <typename Stream>
bool receiveResponse(Stream& stream, int& statusCode) {
    String line = tools::readLine(stream);
    DEBUG("<--- ");
    DEBUGLN(line);

    if (line.startsWith("HTTP/")) {
        int index = line.indexOf(' ');
        statusCode = line.substring(index + 1, index + 4).toInt();
        return statusCode >= 100;
    }
    return false;
}

inline
String decodeUrl(const String& s) {
    String result;
    for (size_t i = 0; i < s.length(); ++i) {
        if (s[i] == '%') {
            long value = 0;
            if (s.length() > i + 2 &&
                    tools::hexToString(s.c_str() + i + 1, 2, value)) {
                result += static_cast<char>(value);
                i += 2;
            }
        } else {
            result += s[i];
        }
    }
    return result;
}

template <typename Stream>
bool receiveRequest(Stream& stream, String& method, String& path) {
    String line = tools::readLine(stream);
    DEBUG("<--- ");
    DEBUGLN(line);
    size_t position = 0;

    method = tools::nextToken(line, ' ', position);
    if (method.length() == 0) {
        return false;
    }

    String rawPath = tools::nextToken(line, ' ', position);
    path = decodeUrl(rawPath);
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

template <typename Stream>
bool readLineEnd(Stream& stream) {
    String crlf = tools::readBuffer(stream, 2);
    if (crlf.length() != 2 || crlf[0] != '\r' || crlf[1] != '\n') {
        DEBUGLN("Invalid CRLF");
        return false;
    }
    return true;
}

template <typename Stream>
bool readChunkedContent(Stream& stream, String& result) {
    while (true) {
        String lengthString = tools::readLine(stream);
        long length = 0;
        if (!tools::hexToString(lengthString, length)) {
            DEBUGLN("Invalid length");
            return false;
        }
        result += tools::readBuffer(stream, length);
        if (!readLineEnd(stream)) {
            return false;
        }
        if (length == 0) {
            return true;
        }
    }
}

template <typename Stream>
bool readHeadersAndContent(Stream& stream, String& content,
        String& connectionHeader) {
    String headerName;
    String headerValue;
    int contentLength = 0;
    bool chunkedEncoding = false;
    while (true) {
        if (!receiveHeader(stream, headerName, headerValue)) {
            return false;
        }
        if (headerName.length() == 0) {
            break;
        }
        if (headerName == "Connection") {
            connectionHeader = headerValue;
        }
        if (headerName == "Content-Length") {
            contentLength = headerValue.toInt();
        }
        if (headerName == "Transfer-Encoding" && headerValue == "chunked") {
            chunkedEncoding = true;
        }
    }

    if (chunkedEncoding) {
        content = "";
        return readChunkedContent(stream, content);
    }
    content = tools::readBuffer(stream, contentLength);
    return true;
}

} // namespace http

#endif // HTTP_HTTP_HPP
