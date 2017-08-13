#include "client.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "wifi.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <limits>

extern "C" {
#include "user_interface.h"
}

namespace {

constexpr unsigned long timeLimit =
        std::numeric_limits<unsigned long>::max() - 60000;

} // unnamed namespace

void setup()
{
    WiFi.mode(WIFI_STA);
    initConfig();
}

void loop()
{
    if (millis() >= timeLimit) {
        debugln("Approaching timer overflow. Rebooting.");
        mqtt::client.disconnect();
        ESP.restart();
    }

    if (wifi::connectIfNeeded(
            globalConfig.wifiSSID, globalConfig.wifiPassword)) {
        mqtt::connectIfNeeded();
    }

    for (const InterfaceConfig& interface : deviceConfig.interfaces) {
        interface.interface->update(Actions{interface.actions});
    }

    mqtt::client.loop();

    delay(1);
}
