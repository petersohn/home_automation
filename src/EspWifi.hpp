#ifndef ESPWIFI_HPP
#define ESPWIFI_HPP

#include "common/Wifi.hpp"

class EspWifi : public Wifi {
public:
    virtual void begin(const std::string& ssid,
        const std::string& password) override;
    virtual WifiStatus getStatus() override;
    virtual std::string getIp() override;
    virtual std::string getMac() override;
};

#endif // ESPWIFI_HPP
