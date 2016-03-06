#include "config/debug.hpp"

#include <ESP8266WiFi.h>
#include <Arduino.h>

namespace wifi {

void connect(const char* ssid, const char* password) {
    DEBUG("Connecting to SSID ");
    DEBUG(ssid);
    DEBUGLN("...");
    WiFi.begin(ssid, password);
    while (true) {
        int status = WiFi.status();
        switch (status) {
        case WL_NO_SHIELD:
            DEBUGLN("Device error");
            // freeze
            while(true) {
                delay(1000);
            }
        case WL_IDLE_STATUS:
        case WL_DISCONNECTED:
            DEBUG('.');
            break;
        case WL_CONNECT_FAILED:
            DEBUGLN("\nConnection failed. Trying again.");
            WiFi.begin(ssid, password);
            delay(1000);
            break;
        case WL_NO_SSID_AVAIL:
            DEBUGLN("\nSSID not found. Trying again.");
            WiFi.begin(ssid, password);
            delay(1000);
            break;
        case WL_CONNECTED:
            DEBUG("\nConnection to wifi successful. IP address = ");
            DEBUGLN(WiFi.localIP());
            return;
        default:
            DEBUG("\nError: ");
            DEBUG(status);
            DEBUGLN(". Trying again.");
            WiFi.begin(ssid, password);
            delay(1000);
            break;
        }
        delay(200);
    }
}

void reconnectIfNeeded(const char* ssid, const char* password) {
}

} // namespace wifi

