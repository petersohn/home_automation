#ifndef DEBUG_HPP
#define DEBUG_HPP

#include "ArduinoJson.hpp"
#include "config.hpp"
#include "print.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

class WiFiDebugger {
public:
    WiFiDebugger(int port) : server(port) {
    }

    void begin() {
        server.begin();
    }

    WiFiClient& getClient() {
        if (client && client.connected()) {
            return client;
        }
        client = server.available();
        return client;
    }

private:
    WiFiServer server;
    WiFiClient client;
};

extern std::unique_ptr<WiFiDebugger> wifiDebugger;

template<typename T>
void debug(const T& value) {
    if (deviceConfig.debug) {
        print(Serial, value);
    }
    if (wifiDebugger) {
        WiFiClient& client = wifiDebugger->getClient();
        if (client) {
            print(client, value);
        }
    }
}

template<typename T = const char*>
void debugln(const T& value = "") {
    if (deviceConfig.debug) {
        println(Serial, value);
    }
    if (wifiDebugger) {
        WiFiClient& client = wifiDebugger->getClient();
        if (client) {
            println(client, value);
        }
    }
}

inline void debugJson(JsonVariant value) {
    if (!deviceConfig.debug) {
        return;
    }

    StaticJsonBuffer<50> buffer;
    JsonArray& array = buffer.createArray();
    array.add(value);
    array.prettyPrintTo(Serial);
}

#endif // DEBUG_HPP
