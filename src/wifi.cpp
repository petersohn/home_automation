#include "debug.hpp"

#include <ESP8266WiFi.h>

namespace wifi {

namespace {

unsigned long nextAttempt = 0;
bool connecting = false;

constexpr int checkInterval = 500;
constexpr int retryInterval = 5000;

} // unnamed namespace

bool connectIfNeeded(const String& ssid, const String& password) {
    int status = WiFi.status();
    if (status == WL_CONNECTED) {
        if (connecting) {
            DEBUG("\nConnection to wifi successful. IP address = ");
            DEBUGLN(WiFi.localIP());
            connecting = false;
        }
        return true;
    }

    auto now = millis();
    if (now < nextAttempt) {
        return false;
    }

    if (!connecting) {
        DEBUG("Connecting to SSID ");
        DEBUG(ssid);
        DEBUGLN("...");
        WiFi.begin(ssid.c_str(), password.c_str());
        connecting = true;
        nextAttempt = now;
        return false;
    }

    switch (status) {
    case WL_NO_SHIELD:
        DEBUGLN("Device error.");
        // never retry
        nextAttempt = static_cast<unsigned long>(-1);
        break;
    case WL_IDLE_STATUS:
    case WL_DISCONNECTED:
        DEBUGLN("Waiting for wifi connection...");
        nextAttempt += checkInterval;
        break;
    case WL_CONNECT_FAILED:
        DEBUGLN("\nConnection failed. Trying again.");
        connecting = false;
        nextAttempt += retryInterval;
        break;
    case WL_NO_SSID_AVAIL:
        DEBUGLN("\nSSID not found. Trying again.");
        connecting = false;
        nextAttempt += retryInterval;
        break;
    case WL_CONNECTED:
        break;
    default:
        DEBUG("\nError: ");
        DEBUG(status);
        DEBUGLN(". Trying again.");
        connecting = false;
        nextAttempt += retryInterval;
        break;
    }
    return status == WL_CONNECTED;
}

} // namespace wifi
