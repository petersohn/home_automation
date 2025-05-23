#ifndef COMMON_WIFI_HPP
#define COMMON_WIFI_HPP

#include <string>

enum class WifiStatus {
    Disconnected,
    Connecting,
    WrongPassword,
    ApNotFound,
    ConnectionFailed,
    Connected,
};

class Wifi {
public:
    virtual void begin(
        const std::string& ssid, const std::string& password) = 0;
    virtual WifiStatus getStatus() = 0;
    virtual std::string getIp() = 0;
    virtual std::string getMac() = 0;

    Wifi() = default;
    Wifi(const Wifi&) = delete;
    Wifi& operator=(const Wifi&) = delete;
};

#endif  // COMMON_WIFI_HPP
