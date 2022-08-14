#ifndef WIFICONNECTION_HPP
#define WIFICONNECTION_HPP

#include <Arduino.h>

extern "C" {
#include <user_interface.h>
}

#include <string>

class WifiConnection {
public:
    void init();
    bool connectIfNeeded(const std::string& ssid, const std::string& password);
private:
    unsigned long nextAttempt = 0;
    bool connecting = false;
    station_status_t lastStatus = (station_status_t)-1;
    unsigned backoffRtcId = 0;
    unsigned long currentBackoff = 0;
    unsigned long lastConnectionFailure = 0;
    unsigned long connectionStarted = 0;

    void setBackoff(unsigned long value);
    void connectionFailed();
};

#endif // WIFICONNECTION_HPP
