#include "debug.hpp"
#include "rtc.hpp"

#include <ESP8266WiFi.h>
extern "C" {
#include <user_interface.h>
}

namespace wifi {

namespace {

unsigned long nextAttempt = 0;
bool connecting = false;
station_status_t lastStatus = (station_status_t)-1;
unsigned backoffRtcId = 0;
unsigned long currentBackoff = 0;
unsigned long lastConnectionFailure = 0;
unsigned long connectionStarted = 0;

constexpr int checkInterval = 500;
constexpr int retryInterval = 5000;
constexpr unsigned long initialBackoff = 60000;
constexpr unsigned long maximumBackoff = 600000;
constexpr unsigned long connectionTimeout = 60000;

void setBackoff(unsigned long value) {
    currentBackoff = value;
    rtcSet(backoffRtcId, currentBackoff);
}

} // unnamed namespace

void init() {
    backoffRtcId = rtcNext();
    currentBackoff = rtcGet(backoffRtcId);
    if (currentBackoff == 0) {
        currentBackoff = initialBackoff;
    }
}

void connectionFailed() {
    auto now = millis();
    if (lastConnectionFailure == 0) {
        debug("\nConnection failed for the first time. Trying again.");
    } else {
        debug("\nConnection failed. Trying again. Rebooting in ");
        debug(now - lastConnectionFailure + currentBackoff);
        debugln(" ms");
        if (lastConnectionFailure < now - currentBackoff) {
            debugln("Failure to connect, rebooting.");
            setBackoff(std::max(currentBackoff * 2, maximumBackoff));
            ESP.restart();
        }
    }
    lastConnectionFailure = now;
    connecting = false;
    nextAttempt += retryInterval;
}

bool connectIfNeeded(const std::string& ssid, const std::string& password) {
    station_status_t s = wifi_station_get_connect_status();
    if (s != lastStatus) {
        debug("asdfsdfsdfsdf status=");
        debugln(s);
        lastStatus = s;
    }
    int status = WiFi.status();
    if (status == WL_CONNECTED) {
        if (connecting) {
            debug("\nConnection to wifi successful. IP address = ");
            debugln(WiFi.localIP());
            connecting = false;
            if (wifiDebugger) {
                wifiDebugger->begin();
            }
            setBackoff(initialBackoff);
            lastConnectionFailure = 0;
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
        connectionStarted = now;
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
        if (connectionStarted < now - connectionTimeout) {
            connectionFailed();
        } else {
            nextAttempt += checkInterval;
        }
        break;
    case WL_CONNECT_FAILED:
        connectionFailed();
        break;
    case WL_NO_SSID_AVAIL:
        debugln("\nSSID not found.");
        connectionFailed();
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
