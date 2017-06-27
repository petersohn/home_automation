#include "client.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "string.hpp"
#include "wifi.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <algorithm>

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
        const char* topic, const unsigned char* payload, unsigned length) {
    String topicStr = topic;
    DEBUGLN("Message received on topic " + topicStr);
    auto interface = std::find_if(
            deviceConfig.interfaces.begin(), deviceConfig.interfaces.end(),
            [&topicStr](const InterfaceConfig& interface) {
                return interface.commandTopic == topicStr;
            });
    if (interface == deviceConfig.interfaces.end()) {
        DEBUGLN("Could not find appropriate interface.");
        return;
    }

    interface->interface->execute(tools::toString((char*)payload, length));
}

String willMessage;

bool connectIfNeeded() {
    if (mqttClient.connected()) {
        return true;
    }
    DEBUGLN("Client status = " + String(mqttClient.state()));
    DEBUGLN("Connecting to MQTT broker...");
    bool result = false;
    if (deviceConfig.availabilityTopic.length() != 0) {
        StaticJsonBuffer<64> buffer;
        JsonObject& message = buffer.createObject();
        message["name"] = deviceConfig.name.c_str();
        message["available"] = false;
        message.printTo(willMessage);
        result = mqttClient.connect(
                deviceConfig.name.c_str(),
                globalConfig.serverUsername.c_str(),
                globalConfig.serverPassword.c_str(),
                deviceConfig.availabilityTopic.c_str(), 0, true,
                willMessage.c_str());
        if (result) {
            String loginMessage;
            message["available"] = true;
            message.printTo(loginMessage);
            mqttClient.publish(deviceConfig.availabilityTopic.c_str(),
                    loginMessage.c_str(), true);
        }
    } else {
        result = mqttClient.connect(
                deviceConfig.name.c_str(),
                globalConfig.serverUsername.c_str(),
                globalConfig.serverPassword.c_str());
    }
    if (result) {
        DEBUGLN("Connection successful.");
        for (const InterfaceConfig& interface : deviceConfig.interfaces) {
            if (interface.commandTopic.length() != 0) {
                mqttClient.subscribe(interface.commandTopic.c_str());
            }
        }
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
    DEBUGLN("Starting up...");
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

    for (const InterfaceConfig& interface : deviceConfig.interfaces) {
        interface.interface->update(Actions{interface.actions});
    }

    mqttClient.loop();

    delay(1);
}
