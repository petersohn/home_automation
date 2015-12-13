#include "wifi/credentials.hpp"
#include "wifi/wifi.hpp"

#include <ESP8266WiFi.h>
#include <Arduino.h>

static int ledPin = 2;

void setup()
{
    Serial.begin(115200);
    Serial.println("START");
    pinMode(ledPin, OUTPUT);
    wifi::connect(wifi::credentials::ssid, wifi::credentials::password);
}

void loop()
{
    digitalWrite(ledPin, HIGH);
    delay(2000);
    digitalWrite(ledPin, LOW);
    delay(2000);
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost.");
        wifi::connect(wifi::credentials::ssid, wifi::credentials::password);
    }
}
