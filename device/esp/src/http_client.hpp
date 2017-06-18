#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

#include "config.hpp"
#include "debug.hpp"
#include "http.hpp"
#include "string.hpp"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

namespace http {

template <typename Connection>
bool connectIfNeeded(Connection& connection, const String& address,
        uint16_t port) {
    if (connection.connected()) {
        return true;
    }

    DEBUG("Connecting to server ");
    DEBUG(address);
    DEBUG(':');
    DEBUG(port);
    DEBUGLN("...");

    if (!connection.connect(address.c_str(), port)) {
        DEBUGLN("Connection failed.");
        return false;
    }
    return true;
}

struct RequestInfo {
    const char* method = "GET";
    String url;
    String password;
    String content;
    bool closeConnection = false;
};

template <typename Connection>
bool sendRequest(Connection& connection, const RequestInfo& requestInfo,
        String& response) {
    IPAddress ip = connection.remoteIP();
    String connectionHeader = requestInfo.closeConnection
            ? "close" : "keep-alive";
    String host = String(ip[0]) + '.' + String(ip[1]) + '.' +
            String(ip[2]) + '.' + String(ip[3]) + ':' +
            String(connection.remotePort());
    http::sendRequest(connection, requestInfo.method, requestInfo.url);
    http::sendHeader(connection, "Host", host);
    http::sendHeader(connection, "Connection", connectionHeader);
    http::sendHeader(connection, "Content-Length",
            String(requestInfo.content.length()));
    http::sendHeader(connection, "Content-Type", "application/json");
    http::sendHeader(connection, "Accept", "application/json");
    http::sendHeader(connection, "x-ha-access", requestInfo.password);
    http::sendHeader(connection, "User-Agent", "curl/7.54.1");
    http::sendHeadersEnd(connection);
    connection.print(requestInfo.content);

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
