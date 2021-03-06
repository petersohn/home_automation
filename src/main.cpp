#include "client.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "rtc.hpp"
#include "wifi.hpp"
#include "common/Action.hpp"
#include "common/Interface.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <cstring>
#include <limits>

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

} // unnamed namespace

void setup()
{
    WiFi.mode(WIFI_STA);
    rtcInit();
    initConfig();
    setDeviceName();
}

void loop()
{
    if (millis() >= timeLimit) {
        debugln("Approaching timer overflow. Rebooting.");
        mqtt::disconnect();
        ESP.restart();
    }

    if (wifi::connectIfNeeded(
            globalConfig.wifiSSID, globalConfig.wifiPassword)) {
        mqtt::loop();
    }

    for (const auto& interface : deviceConfig.interfaces) {
        interface->interface->update(Actions{*interface});
    }

    delay(1);
}
