#include "client.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "string.hpp"
#include "wifi.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <algorithm>
#include <limits>

extern "C" {
#include "user_interface.h"
}

namespace {

constexpr unsigned long timeLimit =
        std::numeric_limits<unsigned long>::max() - 60000;
constexpr unsigned connectionAttemptInterval = 1000;

unsigned long nextConnectionAttempt = 0;


WiFiClient wifiClient;

void onMessageReceived(
        const char* topic, const unsigned char* payload, unsigned length) {
    String topicStr = topic;
    debugln("Message received on topic " + topicStr);
    auto interface = std::find_if(
            deviceConfig.interfaces.begin(), deviceConfig.interfaces.end(),
            [&topicStr](const InterfaceConfig& interface) {
                return interface.commandTopic == topicStr;
            });
    if (interface == deviceConfig.interfaces.end()) {
        debugln("Could not find appropriate interface.");
        return;
    }

    interface->interface->execute(tools::toString((char*)payload, length));
}

String willMessage;

enum class ConnectStatus {
    alreadyConnected,
    connectionSuccessful,
    connectionFailed
};

ConnectStatus connectIfNeeded() {
    if (mqttClient.connected()) {
        return ConnectStatus::alreadyConnected;
    }
    debugln("Client status = " + String(mqttClient.state()));
    debugln("Connecting to MQTT broker...");
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
        debugln("Connection successful.");
        for (const InterfaceConfig& interface : deviceConfig.interfaces) {
            if (interface.commandTopic.length() != 0) {
                mqttClient.subscribe(interface.commandTopic.c_str());
            }
        }
        return ConnectStatus::connectionSuccessful;
    } else {
        debugln("Connection failed.");
        return ConnectStatus::connectionFailed;
    }
}

} // unnamed namespace

void setup()
{
    WiFi.mode(WIFI_STA);
    initConfig();
    mqttClient.setServer(globalConfig.serverAddress.c_str(),
                    globalConfig.serverPort)
            .setCallback(onMessageReceived).setClient(wifiClient);
}

void loop()
{
    if (millis() >= timeLimit) {
        debugln("Approaching timer overflow. Rebooting.");
        mqttClient.disconnect();
        ESP.restart();
    }

    if (wifi::connectIfNeeded(
            globalConfig.wifiSSID, globalConfig.wifiPassword)) {
        if (millis() >= nextConnectionAttempt) {
            switch (connectIfNeeded()) {
            case ConnectStatus::alreadyConnected:
                break;
            case ConnectStatus::connectionSuccessful:
                for (const InterfaceConfig& interface :
                        deviceConfig.interfaces) {
                    interface.interface->start();
                }
                break;
            case ConnectStatus::connectionFailed:
                nextConnectionAttempt = millis() + connectionAttemptInterval;
                break;
            }
        }
    }

    for (const InterfaceConfig& interface : deviceConfig.interfaces) {
        interface.interface->update(Actions{interface.actions});
    }

    mqttClient.loop();

    delay(1);
}
