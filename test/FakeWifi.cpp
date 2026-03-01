#include "FakeWifi.hpp"

void FakeWifi::begin(
    const std::string& /*ssid*/, const std::string& /*password*/) {}

WifiStatus FakeWifi::getStatus() {
    return this->status;
}

std::string FakeWifi::getIp() {
    return this->ip;
}

std::string FakeWifi::getMac() {
    return this->mac;
}
