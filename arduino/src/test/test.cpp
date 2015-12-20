#include "ConnectionPool.hpp"
#include "content.hpp"
#include "login.hpp"
#include "config//credentials.hpp"
#include "config/device.hpp"
#include "config/server.hpp"
#include "http/server.hpp"
#include "wifi/wifi.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

extern "C" {
#include "user_interface.h"
}

static int ledPin = 2;
static WiFiServer httpServer{80};
static unsigned long ledTime;
static uint32_t lastMemory = 0;
static ConnectionPool<WiFiClient> connectionPool;

static void initialize() {
    wifi::connect(wifi::credentials::ssid, wifi::credentials::password);
    while (!sendLogin(server::address, server::port, device::name)) {
        Serial.println("Login failed.");
        delay(2000);
    }
    Serial.println("Login successful");
    httpServer.begin();
}

void setup()
{
    Serial.begin(115200);
    Serial.println("START");
    Serial.print("short = ");
    Serial.println(sizeof(short));
    Serial.print("int = ");
    Serial.println(sizeof(int));
    Serial.print("long = ");
    Serial.println(sizeof(long));
    Serial.print("pointer = ");
    Serial.println(sizeof(void*));
    pinMode(ledPin, OUTPUT);
    ledTime = millis();
}

void loop()
{
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost.");
        initialize();
    }

    WiFiClient connection = httpServer.available();
    if (connection) {
        Serial.print("Incoming connection from ");
        Serial.println(connection.remoteIP());
        connectionPool.add(connection);
    }
    connectionPool.serve(
            [](WiFiClient& client) {
                http::serve<WiFiClient>(client, getContent);
            });

    unsigned long time = millis();
    if (time - ledTime > 1000) {
        ledTime = time;
        int value = digitalRead(ledPin);
        digitalWrite(ledPin, !value);
    }

    uint32_t memory = system_get_free_heap_size();
    if (lastMemory != memory) {
        Serial.print("Memory: ");
        Serial.println(memory);
        lastMemory = memory;
    }

    delayMicroseconds(10000);
}
