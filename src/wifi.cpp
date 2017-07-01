#include "debug.hpp"

#include <ESP8266WiFi.h>

namespace wifi {

void connect(const String& ssid, const String& password) {
    DEBUG("Connecting to SSID ");
    DEBUG(ssid);
    DEBUGLN("...");
    WiFi.begin(ssid.c_str(), password.c_str());
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
            WiFi.begin(ssid.c_str(), password.c_str());
            delay(1000);
            break;
        case WL_NO_SSID_AVAIL:
            DEBUGLN("\nSSID not found. Trying again.");
            WiFi.begin(ssid.c_str(), password.c_str());
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
            WiFi.begin(ssid.c_str(), password.c_str());
            delay(1000);
            break;
        }
        delay(200);
    }
}

} // namespace wifi
