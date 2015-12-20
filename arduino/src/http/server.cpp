#include "server.hpp"

#include "http.hpp"
#include "config/device.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

namespace http {

String getContent(const String& path) {
    if (path == "/") {
        return "{ \"device\": { "
                "\"name\": \"" + String(device::name) + "\", "
                "\"ip\": \"" + String(WiFi.localIP()) + "\", "
                "\"rssi\": " + String(WiFi.RSSI()) + "\"}}";
    } else {
        return "";
    }
}

} // namespace http
