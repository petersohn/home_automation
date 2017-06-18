#include "ConnectionPool.hpp"
#include "client.hpp"
#include "config.hpp"
#include "content.hpp"
#include "debug.hpp"
#include "home_assistant.hpp"
#include "http_client.hpp"
#include "http_server.hpp"
#include "wifi.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

extern "C" {
#include "user_interface.h"
}

static WiFiServer httpServer{80};
static ConnectionPool<WiFiClient> connectionPool;
static WiFiClient httpClient;
static unsigned long nextLoginAttempt = 0;

static constexpr int loginRetryInterval = 5000;
static constexpr const char* statusPath = "/device/status/";

namespace {

bool sendLogin() {
    DEBUGLN("Sending status to server.");
    bool success = true;
    for (InterfaceConfig& interface : deviceConfig.interfaces) {
        success = sendHomeAssistantUpdate(httpClient, interface, false)
                && success;
    }
    return success;
}

void login() {
    if (sendLogin()) {
        DEBUGLN("Sending status successful.");
        nextLoginAttempt = 0;
    } else {
        DEBUGLN("Sending status failed.");
        nextLoginAttempt += loginRetryInterval;
    }
}

void initialize() {
    wifi::connect(globalConfig.wifiSSID, globalConfig.wifiPassword);
    httpServer.begin();
    DEBUGLN("attempting login");
    login();
}

} // unnamed namespace

void setup()
{
    Serial.begin(115200);
    DEBUGLN();
    initConfig();
    DEBUGLN();
}

void loop()
{
    if (WiFi.status() != WL_CONNECTED) {
        DEBUGLN("WiFi connection lost.");
        initialize();
    }

    WiFiClient connection = httpServer.available();
    if (connection) {
        DEBUG("Incoming connection from ");
        DEBUGLN(connection.remoteIP());
        connectionPool.add(connection);
    }
    connectionPool.serve(
            [](WiFiClient& client) {
                http::serve<WiFiClient>(client, getContent);
            });

    for (InterfaceConfig* interface : getModifiedInterfaces()) {
        DEBUG("Modified interface: ");
        DEBUGLN(interface->name);
        sendHomeAssistantUpdate(httpClient, *interface, true);
    }

    if (nextLoginAttempt != 0 && millis() >= nextLoginAttempt) {
        DEBUGLN("reattempting login");
        login();
    }

    delay(5);
}
