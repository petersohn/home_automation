#include "debug.hpp"

#include <ESP8266WiFi.h>

namespace wifi {

namespace {

unsigned long nextAttempt = 0;
bool connecting = false;

constexpr int checkInterval = 500;
constexpr int retryInterval = 5000;

} // unnamed namespace

bool connectIfNeeded(const std::string& ssid, const std::string& password) {
    int status = WiFi.status();
    if (status == WL_CONNECTED) {
        if (connecting) {
            debug("\nConnection to wifi successful. IP address = ");
            debugln(WiFi.localIP());
            connecting = false;
            if (wifiDebugger) {
                wifiDebugger->begin();
            }
        }
        return true;
    }

    auto now = millis();
    if (now < nextAttempt) {
        return false;
    }

    if (!connecting) {
        debug("Connecting to SSID ");
        debug(ssid);
        debugln("...");
        WiFi.begin(ssid.c_str(), password.c_str());
        connecting = true;
        nextAttempt = now;
        return false;
    }

    switch (status) {
    case WL_NO_SHIELD:
        debugln("Device error.");
        // never retry
        nextAttempt = static_cast<unsigned long>(-1);
        break;
    case WL_IDLE_STATUS:
    case WL_DISCONNECTED:
        debugln("Waiting for wifi connection...");
        nextAttempt += checkInterval;
        break;
    case WL_CONNECT_FAILED:
        debugln("\nConnection failed. Trying again.");
        connecting = false;
        nextAttempt += retryInterval;
        break;
    case WL_NO_SSID_AVAIL:
        debugln("\nSSID not found. Trying again.");
        connecting = false;
        nextAttempt += retryInterval;
        break;
    case WL_CONNECTED:
        break;
    default:
        debug("\nError: ");
        debug(status);
        debugln(". Trying again.");
        connecting = false;
        nextAttempt += retryInterval;
        break;
    }
    return status == WL_CONNECTED;
}

} // namespace wifi
