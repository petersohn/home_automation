#include <ESP8266WiFi.h>
#include <Arduino.h>

namespace wifi {

void connect(const char* ssid, const char* password) {
    Serial.print("Connecting to SSID ");
    Serial.print(ssid);
    Serial.println("...");
    WiFi.begin(ssid, password);
    while (true) {
        int status = WiFi.status();
        switch (status) {
        case WL_NO_SHIELD:
            Serial.println("Device error");
            // freeze
            while(true) {
                delay(1000);
            }
        case WL_IDLE_STATUS:
        case WL_DISCONNECTED:
            Serial.print('.');
            break;
        case WL_CONNECT_FAILED:
            Serial.println("\nConnection failed. Trying again.");
            WiFi.begin(ssid, password);
            delay(1000);
            break;
        case WL_NO_SSID_AVAIL:
            Serial.println("\nSSID not found. Trying again.");
            WiFi.begin(ssid, password);
            delay(1000);
            break;
        case WL_CONNECTED:
            Serial.print("\nConnection to wifi successful. IP address = ");
            Serial.println(WiFi.localIP());
            return;
        default:
            Serial.print("\nError: ");
            Serial.print(status);
            Serial.println(". Trying again.");
            WiFi.begin(ssid, password);
            delay(1000);
            break;
        }
        delay(200);
    }
}

void reconnectIfNeeded(const char* ssid, const char* password) {
}

} // namespace wifi

