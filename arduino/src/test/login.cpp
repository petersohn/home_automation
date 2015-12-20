#include "login.hpp"
#include "http/http.hpp"
#include "tools/string.hpp"

#include <ESP8266WiFi.h>
#include <Arduino.h>

bool sendLogin(const char* address, int port, const char* name) {
    Serial.print("Connecting to ");
    Serial.print(address);
    Serial.print(":");
    Serial.print(port);
    Serial.println("...");
    WiFiClient client;
    if (!client.connect(address, port)) {
        Serial.println("Connection failed.");
        return false;
    }

    String content = "{ \"name\": \"" + String(name) + "\" }";

    http::sendRequest(client, "POST", "/login");
    http::sendHeader(client, "Host", String(address) + ":" + String(port));
    http::sendHeader(client, "Connection", "close");
    http::sendHeader(client, "Content-Length", content.length());
    http::sendHeadersEnd(client);
    client.print(content);

    int statusCode = 0;
    return http::receiveAnswerStatus(client, statusCode) && 
            statusCode >= 200 && statusCode < 300;
}

