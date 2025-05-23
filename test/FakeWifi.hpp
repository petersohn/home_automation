#ifndef TEST_FAKEWIFI_HPP
#define TEST_FAKEWIFI_HPP

#include "common/Wifi.hpp"

class FakeWifi : public Wifi {
public:
    WifiStatus status = WifiStatus::Connected;
    std::string ip = "10.0.0.1";
    std::string mac = "00:00:00:00:00:00";

    virtual void begin(
        const std::string& ssid, const std::string& password) override;
    virtual WifiStatus getStatus() override;
    virtual std::string getIp() override;
    virtual std::string getMac() override;
};

#endif  // TEST_FAKEWIFI_HPP
