#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

#include "http/http.hpp"
#include "tools/string.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

namespace http {

template <typename Connection>
bool connectIfNeeded(Connection& connection, const char* address,
        uint16_t port) {
    if (connection.connected()) {
        return true;
    }

    Serial.print("Connecting to server ");
    Serial.print(address);
    Serial.print(':');
    Serial.print(port);
    Serial.println("...");

    if (!connection.connect(address, port)) {
        Serial.println("Connection failed.");
        return false;
    }
    return true;
}

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

    if (!readHeadersAndContent(connection, response, connectionHeader)) {
        connection.stop();
        return false;
    }
    if (connectionHeader != "keep-alive") {
        connection.stop();
    }
    return statusCode >= 200 && statusCode < 300;
}

} // namespace http


#endif // HTTP_CLIENT_HPP
