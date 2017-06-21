#include "client.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "string.hpp"
#include "wifi.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

extern "C" {
#include "user_interface.h"
}

namespace {

constexpr unsigned connectionAttemptInterval = 1000;
unsigned long nextConnectionAttempt = 0;

WiFiClient wifiClient;

void initialize() {
    wifi::connect(globalConfig.wifiSSID, globalConfig.wifiPassword);
}

void onMessageReceived(
        const char* /*topic*/, const unsigned char* payload, unsigned length) {
    mqttClient.publish("foo/baz", payload, length);
}

bool connectIfNeeded() {
    if (mqttClient.connected()) {
        return true;
    }
    DEBUGLN("Connecting to MQTT broker...");
    bool result = mqttClient.connect(
            deviceConfig.name.c_str(),
            globalConfig.serverUsername.c_str(),
            globalConfig.serverPassword.c_str());
    if (result) {
        DEBUGLN("Connection successful.");
        mqttClient.subscribe("foo/bar");
    } else {
        DEBUGLN("Connection failed.");
    }
    return result;
}

} // unnamed namespace

void setup()
{
    Serial.begin(115200);
    DEBUGLN();
    initConfig();
    mqttClient.setServer(globalConfig.serverAddress.c_str(),
                    globalConfig.serverPort)
            .setCallback(onMessageReceived).setClient(wifiClient);
}

void loop()
{
    if (WiFi.status() != WL_CONNECTED) {
        DEBUGLN("WiFi connection lost.");
        initialize();
    }

    if (millis() >= nextConnectionAttempt) {
        if (!connectIfNeeded()) {
            nextConnectionAttempt = millis() + connectionAttemptInterval;
        }
    }

    delay(5);
}
