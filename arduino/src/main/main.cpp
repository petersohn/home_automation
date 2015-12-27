#include "ConnectionPool.hpp"
#include "client.hpp"
#include "content.hpp"
#include "config//credentials.hpp"
#include "config/device.hpp"
#include "config/server.hpp"
#include "http/client.hpp"
#include "http/server.hpp"
#include "wifi/wifi.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

extern "C" {
#include "user_interface.h"
}

static WiFiServer httpServer{80};
static ConnectionPool<WiFiClient> connectionPool;
static WiFiClient httpClient;

namespace {

bool sendLogin() {
    if (!httpClient.connect(server::address, server::port)) {
        return false;
    }
    String returnContent;
    return http::sendRequest(httpClient, "POST", "/login",
            getLoginContent(), returnContent, false);
}

void initialize() {
    wifi::connect(wifi::credentials::ssid, wifi::credentials::password);
    while (!sendLogin()) {
        Serial.println("Login failed.");
        delay(2000);
    }
    Serial.println("Login successful");
    httpServer.begin();
}

} // unnamed namespace

void setup()
{
    Serial.begin(115200);
    Serial.println();

    for (device::Pin& pin : device::pins) {
        pinMode(pin.number, (pin.output ? OUTPUT : INPUT));
        pin.status = digitalRead(pin.number);
    }
}

void loop()
{
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost.");
        initialize();
    }

    WiFiClient connection = httpServer.available();
    if (connection) {
        Serial.print("Incoming connection from ");
        Serial.println(connection.remoteIP());
        connectionPool.add(connection);
    }
    connectionPool.serve(
            [](WiFiClient& client) {
                http::serve<WiFiClient>(client, getContent);
            });

    String modifiedPinsContent = getModifiedPinsContent();
    if (modifiedPinsContent.length() != 0) {
        if (httpClient.connect(server::address, server::port)) {
            String returnContent;
            http::sendRequest(httpClient, "POST", "/status",
                    modifiedPinsContent, returnContent, false);
        }
    }

    delay(10);
}
