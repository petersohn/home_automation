#ifndef DEBUG_HPP
#define DEBUG_HPP

#include "config.hpp"
#include "print.hpp"
#include "StringStream.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

class WiFiDebugger {
public:
    WiFiDebugger(int port) : server(port) {
    }

    void begin() {
        server.begin();
    }

    WiFiClient& getClient();

private:
    WiFiServer server;
    WiFiClient client;
};

extern std::unique_ptr<WiFiDebugger> wifiDebugger;

class MqttDebugger {
public:
    MqttDebugger(const std::string& topic) : topic(topic) {}

    template<typename T>
    void add(const T& value) {
        if (!sending) {
            print(currentLine, value);
        }
    }

    void send();

private:
    const std::string topic;
    StringStream currentLine;
    bool sending = false;
};

extern std::unique_ptr<MqttDebugger> mqttDebugger;

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

    if (mqttDebugger) {
        mqttDebugger->add(value);
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

    if (mqttDebugger) {
        mqttDebugger->add(value);
        mqttDebugger->send();
    }
}

#endif // DEBUG_HPP
