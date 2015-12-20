#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

#include "http/http.hpp"
#include "tools/string.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

namespace http {

template <typename Connection>
bool sendRequest(Connection& connection, const char* method, const char* path,
        const String& content, String& response, bool closeConnection) {
    IPAddress ip = connection.remoteIP();
    String connectionHeader = closeConnection ? "close" : "keep-alive";
    String host = String(ip[0]) + '.' + String(ip[1]) + '.' +
            String(ip[2]) + '.' + String(ip[3]) + ':' +
            String(connection.remotePort());
    http::sendRequest(connection, method, path);
    http::sendHeader(connection, "Host", host);
    http::sendHeader(connection, "Connection", connectionHeader);
    http::sendHeader(connection, "Content-Length", content.length());
    http::sendHeader(connection, "Content-Type", "application/json");
    http::sendHeadersEnd(connection);
    connection.print(content);

    int statusCode = 0;
    if (!http::receiveResponse(connection, statusCode)) {
        connection.stop();
        return false;
    }

    String headerName;
    String headerValue;
    int contentLength = 0;
    while (true) {
        if (!receiveHeader(connection, headerName, headerValue)) {
            continue;
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
    }

    response = tools::readBuffer(connection, contentLength);
    if (connectionHeader != "keep-alive") {
        connection.stop();
    }
    return statusCode >= 200 && statusCode < 300;
}

} // namespace http


#endif // HTTP_CLIENT_HPP
