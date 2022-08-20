#include "WifiConnection.hpp"

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
    auto now = esp.millis();
    debug << "Connection failed";
    if (lastConnectionFailure == 0) {
        debug << " for the first time. Trying again."
            << std::endl;
        lastConnectionFailure = now;
    } else {
        if (now > lastConnectionFailure + currentBackoff) {
            debug << ", rebooting." << std::endl;
            setBackoff(std::min(currentBackoff * 2, maximumBackoff));
            esp.restart(true);
        }
        debug << ", trying again. Rebooting in "
            << static_cast<long>(lastConnectionFailure) + currentBackoff - now
            << " ms" << std::endl;
    }
    connecting = false;
    nextAttempt += retryInterval;
}

bool WifiConnection::connectIfNeeded(const std::string& ssid, const std::string& password) {
    auto status = wifi.getStatus();
    if (status == WifiStatus::Connected) {
        if (connecting) {
            debug << "\nConnection to wifi successful. IP address = "
                << wifi.getIp() << std::endl;
            connecting = false;
//            if (wifiDebugger) {
//                wifiDebugger->begin();
//            }
            setBackoff(initialBackoff);
            lastConnectionFailure = 0;
        }
        return true;
    }

    auto now = esp.millis();
    if (now < nextAttempt) {
        return false;
    }

    if (!connecting) {
        debug << "Connecting to SSID " << ssid << "..." << std::endl;
        wifi.begin(ssid, password);
        connecting = true;
        nextAttempt = now;
        connectionStarted = now;
        return false;
    }

    switch (status) {
    case WifiStatus::Connected:
        break;
    case WifiStatus::Connecting:
        debug << "Waiting for wifi connection ("
            << connectionStarted + connectionTimeout - now
            << " remaining)..." << std::endl;
        if (connecting && now > connectionStarted + connectionTimeout) {
            connectionFailed();
        } else {
            nextAttempt += checkInterval;
        }
        break;
    case WifiStatus::Disconnected:
        debug << "Not connected. This should not happen." << std::endl;
        connectionFailed();
        break;
    case WifiStatus::ConnectionFailed:
        connectionFailed();
        break;
    case WifiStatus::ApNotFound:
        debug << "SSID not found." << std::endl;
        connectionFailed();
        break;
    case WifiStatus::WrongPassword:
        debug << "Wrong password." << std::endl;
        connectionFailed();
        break;
    default:
        debug << "This should not happen." << std::endl;
        connectionFailed();
        break;
    }
    return status == WifiStatus::Connected;
}
