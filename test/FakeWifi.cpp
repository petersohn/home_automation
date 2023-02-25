#include "FakeWifi.hpp"

void FakeWifi::begin(
        const std::string& /*ssid*/, const std::string& /*password*/) {

}

WifiStatus FakeWifi::getStatus() {
    return status;
}

std::string FakeWifi::getIp() {
    return ip;
}

std::string FakeWifi::getMac() {
    return mac;
}

