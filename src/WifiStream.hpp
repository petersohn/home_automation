#ifndef WIFISTREAM_HPP
#define WIFISTREAM_HPP

#include <streambuf>
#include <ESP8266WiFi.h>

class WifiStreambuf: public std::streambuf {
public:
    WifiStreambuf(int port): server(port) {
        server.begin();
    }

protected:
    virtual int overflow(int ch) override;

private:
    WiFiServer server;
    WiFiClient client;
    unsigned long lastChecked = 0;

    void initClientIfNeeded();
};

#endif // WIFISTREAM_HPP
