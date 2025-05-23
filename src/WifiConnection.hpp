#ifndef WIFICONNECTION_HPP
#define WIFICONNECTION_HPP

#include <ostream>
#include <string>

#include "common/Backoff.hpp"
#include "common/EspApi.hpp"
#include "common/Wifi.hpp"

class WifiConnection {
public:
    WifiConnection(
        std::ostream& debug, EspApi& esp, Backoff& backoff, Wifi& wifi)
        : debug(debug), esp(esp), backoff(backoff), wifi(wifi) {}
    bool connectIfNeeded(const std::string& ssid, const std::string& password);

private:
    std::ostream& debug;
    EspApi& esp;
    Backoff& backoff;
    Wifi& wifi;

    unsigned long nextAttempt = 0;
    bool connecting = false;
    bool connected = false;
    unsigned long connectionStarted = 0;

    void connectionFailed();
};

#endif  // WIFICONNECTION_HPP
