#include "WifiConnection.hpp"

namespace {

constexpr int checkInterval = 1000;
constexpr int retryInterval = 5000;
constexpr unsigned long initialBackoff = 120000;
constexpr unsigned long maximumBackoff = 1200000;
constexpr unsigned long connectionTimeout = 40000;

} // unnamed namespace

void WifiConnection::connectionFailed() {
    backoff.bad();
    connecting = false;
    nextAttempt += retryInterval;
}

bool WifiConnection::connectIfNeeded(const std::string& ssid, const std::string& password) {
    auto status = wifi.getStatus();
    if (status == WifiStatus::Connected) {
        if (!connected) {
            debug << "\nConnection to wifi successful. IP address = "
                << wifi.getIp() << std::endl;
            connected = true;
            backoff.good();
        }
        connecting = false;
        return true;
    }

    connected = false;

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
        if (now > connectionStarted + connectionTimeout) {
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
