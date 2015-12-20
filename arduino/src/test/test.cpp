#include "login.hpp"
#include "config//credentials.hpp"
#include "config/device.hpp"
#include "config/server.hpp"
#include "http/server.hpp"
#include "wifi/wifi.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

static int ledPin = 2;
static WiFiServer httpServer{80};

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
    pinMode(ledPin, OUTPUT);
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
        http::serve(connection);
        int value = digitalRead(ledPin);
        digitalWrite(ledPin, !value);
    }
    delayMicroseconds(10000);
}
