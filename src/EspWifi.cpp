#include "EspWifi.hpp"

#include <ESP8266WiFi.h>

#include <cstring>

#include "tools/string.hpp"

extern "C" {
#include <user_interface.h>
}

void EspWifi::begin(const std::string& ssid, const std::string& password) {
    WiFi.begin(ssid.c_str(), password.c_str());
}

WifiStatus EspWifi::getStatus() {
    auto status = wifi_station_get_connect_status();
    switch (status) {
    case STATION_IDLE:
        return WifiStatus::Disconnected;
    case STATION_CONNECTING:
        return WifiStatus::Connecting;
    case STATION_WRONG_PASSWORD:
        return WifiStatus::WrongPassword;
    case STATION_NO_AP_FOUND:
        return WifiStatus::ApNotFound;
    case STATION_CONNECT_FAIL:
        return WifiStatus::ConnectionFailed;
    case STATION_GOT_IP:
        return WifiStatus::Connected;
    default:
        // should not happen
        return WifiStatus::Disconnected;
    }
}

std::string EspWifi::getIp() {
    return WiFi.localIP().toString().c_str();
}

std::string EspWifi::getMac() {
    std::uint8_t mac[6];
    WiFi.macAddress(mac);
    tools::Join result{":"};
    for (int i = 0; i < 6; ++i) {
        result.add(tools::intToString(mac[i], 16));
    }
    return result.get();
}
