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
static unsigned long nextHeartbeat = 0;
static String heartbeatType = "login";

static constexpr int heartbeatInterval = 60000;
static constexpr int heartbeatRetryInterval = 5000;
static constexpr const char* statusPath = "/device/status/";

namespace {

bool sendHeartbeat(const String& type) {
    if (!http::connectIfNeeded(httpClient, server::address, server::port)) {
        return false;
    }
    String returnContent;
    return http::sendRequest(httpClient, "POST", statusPath,
            getFullStatus(type), returnContent, false);
}

void heartbeat() {
    unsigned long now = millis();
    if (sendHeartbeat(heartbeatType)) {
        nextHeartbeat = now + heartbeatInterval;
        heartbeatType = "heartbeat";
    } else {
        nextHeartbeat = now + heartbeatRetryInterval;
    }
}

void initialize() {
    wifi::connect(wifi::credentials::ssid, wifi::credentials::password);
    heartbeat();
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
        pin.lastSeen = millis();
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

    String modifiedPinsContent = getModifiedPinsContent("event");
    if (modifiedPinsContent.length() != 0) {
        if (http::connectIfNeeded(httpClient, server::address, server::port)) {
            String returnContent;
            if (http::sendRequest(httpClient, "POST", statusPath,
                    modifiedPinsContent, returnContent, false)) {
                nextHeartbeat = millis() + heartbeatInterval;
            }
        }
    }

    if (millis() > nextHeartbeat) {
        heartbeat();
    }

    delay(10);
}
