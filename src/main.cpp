#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <cstring>
#include <limits>
#include <ostream>
#include <sstream>

#include "DebugStream.hpp"
#include "EspApiImpl.hpp"
#include "EspRtc.hpp"
#include "EspWifi.hpp"
#include "MqttConnectionImpl.hpp"
#include "MqttStream.hpp"
#include "WifiConnection.hpp"
#include "WifiStream.hpp"
#include "common/Action.hpp"
#include "common/BackoffImpl.hpp"
#include "common/Interface.hpp"
#include "common/MqttClient.hpp"
#include "config.hpp"

extern "C" {
#include "user_interface.h"
}

namespace {

constexpr unsigned long timeLimit =
    std::numeric_limits<unsigned long>::max() - 60000;

void setDeviceName() {
    static char* name = nullptr;
    if (name) {
        return;
    }

    // This function accepts a char*, and it is not documented if copies the
    // name or not. To be safe, the string is made permanent.
    name = new char[deviceConfig.name.length() + 5];
    std::strcpy(name, "ESP_");
    std::strcat(name, deviceConfig.name.c_str());
    wifi_station_set_hostname(name);
}

DebugStreambuf debugStream;
std::ostream debug(&debugStream);
EspApiImpl esp;
EspRtc rtc;
EspWifi wifi;

BackoffImpl wifiBackoff(debug, "wifi: ", esp, rtc, 150000, 1500000);
WifiConnection wifiConnection(debug, esp, wifiBackoff, wifi);

Lock mqttLock;
BackoffImpl mqttBackoff(debug, "mqtt: ", esp, rtc, 210000, 2100000);
MqttConnectionImpl mqttConnection(debug, mqttLock);
MqttClient mqttClient(debug, esp, wifi, mqttBackoff, mqttConnection, []() {
    for (const auto& interface : deviceConfig.interfaces) {
        interface->interface->start();
    }
});
std::unique_ptr<WifiStreambuf> wifiStream;
std::unique_ptr<MqttStreambuf> mqttStream;

}  // unnamed namespace

void setup() {
    WiFi.mode(WIFI_STA);
    initConfig(debug, debugStream, esp, rtc, mqttClient);
    mqttClient.setConfig(MqttConfig{
        deviceConfig.name,
        std::move(globalConfig.servers),
        std::move(deviceConfig.topics),
    });
    setDeviceName();

    if (deviceConfig.debugTopic != "") {
        mqttStream = std::make_unique<MqttStreambuf>(
            mqttLock, mqttClient, deviceConfig.debugTopic);
        debugStream.add(mqttStream.get());
    }
}

void loop() {
    if (millis() >= timeLimit) {
        debug << "Approaching timer overflow. Rebooting." << std::endl;
        mqttClient.disconnect();
        esp.restart(true);
    }

    const bool isConnected = wifiConnection.connectIfNeeded(
        globalConfig.wifiSSID, globalConfig.wifiPassword);
    if (isConnected) {
        if (!wifiStream && deviceConfig.debugPort != 0) {
            wifiStream =
                std::make_unique<WifiStreambuf>(deviceConfig.debugPort);
            debugStream.add(wifiStream.get());
        }
        mqttClient.loop();
    } else if (wifiStream) {
        debugStream.remove(wifiStream.get());
        wifiStream.reset();
    }

    for (const auto& interface : deviceConfig.interfaces) {
        interface->interface->update(Actions{*interface});
    }

    delay(1);
}
