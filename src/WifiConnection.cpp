#include "WifiConnection.hpp"

namespace {

constexpr int checkInterval = 1000;
constexpr int retryInterval = 5000;
constexpr unsigned long connectionTimeout = 60000;

}  // unnamed namespace

void WifiConnection::connectionFailed() {
    this->backoff.bad();
    this->connecting = false;
    this->nextAttempt += retryInterval;
}

bool WifiConnection::connectIfNeeded(
    const std::string& ssid, const std::string& password) {
    auto status = this->wifi.getStatus();
    if (status == WifiStatus::Connected) {
        if (!this->connected) {
            this->debug << "\nConnection to wifi successful. IP address = "
                        << this->wifi.getIp() << std::endl;
            this->connected = true;
            this->backoff.good();
        }
        this->connecting = false;
        return true;
    }

    this->connected = false;

    auto now = this->esp.millis();
    if (now < this->nextAttempt) {
        return false;
    }

    if (!this->connecting) {
        this->debug << "Connecting to SSID " << ssid << "..." << std::endl;
        this->wifi.begin(ssid, password);
        this->connecting = true;
        this->nextAttempt = now;
        this->connectionStarted = now;
        return false;
    }

    switch (status) {
    case WifiStatus::Connected:
        break;
    case WifiStatus::Connecting:
        this->debug << "Waiting for wifi connection ("
                    << this->connectionStarted + connectionTimeout - now
                    << " remaining)..." << std::endl;
        if (now > this->connectionStarted + connectionTimeout) {
            connectionFailed();
        } else {
            this->nextAttempt += checkInterval;
        }
        break;
    case WifiStatus::Disconnected:
        this->debug << "Not connected. This should not happen." << std::endl;
        connectionFailed();
        break;
    case WifiStatus::ConnectionFailed:
        connectionFailed();
        break;
    case WifiStatus::ApNotFound:
        this->debug << "SSID not found." << std::endl;
        connectionFailed();
        break;
    case WifiStatus::WrongPassword:
        this->debug << "Wrong password." << std::endl;
        connectionFailed();
        break;
    default:
        this->debug << "This should not happen." << std::endl;
        connectionFailed();
        break;
    }
    return status == WifiStatus::Connected;
}
