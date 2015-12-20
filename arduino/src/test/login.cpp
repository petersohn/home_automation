#include "login.hpp"
#include "http/client.hpp"
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
    String returnContent;
    return http::sendRequest(client, "POST", "/login", content, returnContent,
            true);
}

