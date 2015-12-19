#include "login.hpp"
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
    String dataToSend = "POST /login HTTP/1.1\r\n"
            "Host: " + String(address) + ":" + String(port) + "\r\n"
            "Connection: close\r\n" +
            "Content-Length: " + String(content.length()) + "\r\n\r\n" +
            content;
    Serial.println("");
    Serial.println(dataToSend);
    client.write(dataToSend.c_str(), dataToSend.length());

    bool result = false;
    while (true) {
        String line = tools::readLine(client);
        Serial.println(line);
        if (line.length() == 0) {
            return result;
        }

        if (line.startsWith("HTTP")) {
            int index = line.indexOf(' ');
            int statusCode = line.substring(index + 1, index + 4).toInt();
            Serial.print("--> Status code: ");
            Serial.println(statusCode);
            result = statusCode >= 100 && statusCode < 300;
        }
    }
}

