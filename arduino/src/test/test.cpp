#include "device.hpp"
#include "login.hpp"
#include "server.hpp"
#include "wifi/credentials.hpp"
#include "wifi/wifi.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

static int ledPin = 2;

static void initialize() {
    wifi::connect(wifi::credentials::ssid, wifi::credentials::password);
    while (!sendLogin(server::address, server::port, device::name)) {
        Serial.println("Login failed.");
        delay(2000);
    }
    Serial.println("Login successful");
}

void setup()
{
    Serial.begin(115200);
    Serial.println("START");
    pinMode(ledPin, OUTPUT);
    initialize();
}

void loop()
{
    digitalWrite(ledPin, HIGH);
    delay(2000);
    digitalWrite(ledPin, LOW);
    delay(2000);
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost.");
        initialize();
    }
}
