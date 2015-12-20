#include "content.hpp"

#include "config/device.hpp"
#include "http/http.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

extern "C" {
#include "user_interface.h"
}

String getContent(const String& path, const String& /*content*/) {
    if (path == "/") {
        IPAddress ip = WiFi.localIP();
        String ipString = String(ip[0]) + '.' + String(ip[1]) + '.' +
                String(ip[2]) + '.' + String(ip[3]);
        return "{ \"device\": { "
                "\"name\": \"" + String(device::name) + "\", "
                "\"ip\": \"" + ipString + "\", "
                "\"memory\": " + String(system_get_free_heap_size()) + ", "
                "\"rssi\": " + String(WiFi.RSSI()) + "}}";
    } else {
        return "";
    }
}
