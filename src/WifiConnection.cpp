#include "WifiConnection.hpp"

#include <ESP8266WiFi.h>
extern "C" {
#include <user_interface.h>
}

namespace {

constexpr int checkInterval = 500;
constexpr int retryInterval = 5000;
constexpr unsigned long initialBackoff = 60000;
constexpr unsigned long maximumBackoff = 600000;
constexpr unsigned long connectionTimeout = 40000;

} // unnamed namespace

void WifiConnection::setBackoff(unsigned long value) {
    debug << "New backoff: " << value << std::endl;
    currentBackoff = value;
    rtc.set(backoffRtcId, currentBackoff);
}

void WifiConnection::init() {
    backoffRtcId = rtc.next();
    currentBackoff = rtc.get(backoffRtcId);
    if (currentBackoff == 0) {
        currentBackoff = initialBackoff;
    }
    debug << "Initial backoff: " << currentBackoff << std::endl;
}

void WifiConnection::connectionFailed() {
    auto now = millis();
    if (lastConnectionFailure == 0) {
        debug << "\nConnection failed for the first time. Trying again."
            << std::endl;
    } else {
        debug << "\nConnection failed. Trying again. Rebooting in "
            << static_cast<long>(lastConnectionFailure) + currentBackoff - now
            << " ms (backoff=" << currentBackoff << " ms)" << std::endl;
        if (now > lastConnectionFailure + currentBackoff) {
            debug << "Failure to connect, rebooting." << std::endl;
            setBackoff(std::min(currentBackoff * 2, maximumBackoff));
            ESP.restart();
        }
    }
    lastConnectionFailure = now;
    connecting = false;
    nextAttempt += retryInterval;
}

bool WifiConnection::connectIfNeeded(const std::string& ssid, const std::string& password) {
    int status = WiFi.status();
    if (status == WL_CONNECTED) {
        if (connecting) {
            debug << "\nConnection to wifi successful. IP address = "
                << WiFi.localIP().toString().c_str() << std::endl;
            connecting = false;
//            if (wifiDebugger) {
//                wifiDebugger->begin();
//            }
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
        debug << "Connecting to SSID " << ssid << "..." << std::endl;
        WiFi.begin(ssid.c_str(), password.c_str());
        connecting = true;
        nextAttempt = now;
        connectionStarted = now;
        return false;
    }

    switch (status) {
    case WL_NO_SHIELD:
        debug << "Device error." << std::endl;
        // never retry
        nextAttempt = static_cast<unsigned long>(-1);
        break;
    case WL_IDLE_STATUS:
    case WL_DISCONNECTED:
        debug << "Waiting for wifi connection ("
            << connectionStarted + connectionTimeout - now
            << " remaining)..." << std::endl;
        if (connecting && now > connectionStarted + connectionTimeout) {
            connectionFailed();
        } else {
            nextAttempt += checkInterval;
        }
        break;
    case WL_CONNECT_FAILED:
        connectionFailed();
        break;
    case WL_NO_SSID_AVAIL:
        debug << "SSID not found." << std::endl;
        connectionFailed();
        break;
    case WL_CONNECTED:
        break;
    default:
        connectionFailed();
        break;
    }
    return status == WL_CONNECTED;
}
