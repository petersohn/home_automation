#include "client.hpp"
#include "config.hpp"
#include "DebugStream.hpp"
#include "EspApiImpl.hpp"
#include "EspRtc.hpp"
#include "EspWifi.hpp"
#include "WifiStream.hpp"
#include "MqttStream.hpp"
#include "MqttConnectionImpl.hpp"
#include "WifiConnection.hpp"
#include "common/Action.hpp"
#include "common/Backoff.hpp"
#include "common/Interface.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <cstring>
#include <sstream>
#include <limits>
#include <ostream>

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

Backoff wifiBackoff(debug, "wifi: ", esp, rtc, 120000, 1200000);
WifiConnection wifiConnection(debug, esp, wifiBackoff, wifi);

Backoff mqttBackoff(debug, "mqtt: ", esp, rtc, 180000, 1800000);
MqttConnectionImpl mqttConnection;
MqttClient mqttClient(debug, esp, wifi, mqttBackoff, mqttConnection);
std::unique_ptr<WifiStreambuf> wifiStream;
std::unique_ptr<MqttStreambuf> mqttStream;

} // unnamed namespace

void setup()
{
    WiFi.mode(WIFI_STA);
    initConfig(debug, debugStream, esp, rtc, mqttClient);
    setDeviceName();

    if (deviceConfig.debugTopic != "") {
        mqttStream = std::make_unique<MqttStreambuf>(
            mqttClient, deviceConfig.debugTopic);
        debugStream.add(mqttStream.get());
    }
}

void loop()
{
    if (millis() >= timeLimit) {
        debug << "Approaching timer overflow. Rebooting." << std::endl;
        mqttClient.disconnect();
        esp.restart(true);
    }

    const bool isConnected = wifiConnection.connectIfNeeded(
        globalConfig.wifiSSID, globalConfig.wifiPassword);
    if (isConnected) {
        if (!wifiStream && deviceConfig.debugPort != 0) {
            wifiStream = std::make_unique<WifiStreambuf>(
                deviceConfig.debugPort);
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
