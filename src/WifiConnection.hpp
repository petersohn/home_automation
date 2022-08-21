#ifndef WIFICONNECTION_HPP
#define WIFICONNECTION_HPP

#include "common/rtc.hpp"
#include "common/EspApi.hpp"
#include "common/Wifi.hpp"

#include <string>
#include <ostream>

class WifiConnection {
public:
    WifiConnection(std::ostream& debug, EspApi& esp, Rtc& rtc, Wifi& wifi)
        : debug(debug), esp(esp), rtc(rtc), wifi(wifi) {}
    void init();
    bool connectIfNeeded(const std::string& ssid, const std::string& password);
private:
    std::ostream& debug;
    EspApi& esp;
    Rtc& rtc;
    Wifi& wifi;

    unsigned long nextAttempt = 0;
    bool connecting = false;
    bool connected = false;
    unsigned backoffRtcId = 0;
    unsigned long currentBackoff = 0;
    unsigned long lastConnectionFailure = 0;
    unsigned long connectionStarted = 0;

    void setBackoff(unsigned long value);
    void connectionFailed();
};

#endif // WIFICONNECTION_HPP
