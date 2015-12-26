#include "content.hpp"

#include "config/device.hpp"
#include "http/http.hpp"
#include "tools/string.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <algorithm>

extern "C" {
#include "user_interface.h"
}

namespace {

String getDeviceInfo() {
    IPAddress ip = WiFi.localIP();
    String ipString = String(ip[0]) + '.' + String(ip[1]) + '.' +
            String(ip[2]) + '.' + String(ip[3]);
    return "{ "
        "\"name\": \"" + String(device::name) + "\", "
        "\"ip\": \"" + ipString + "\", "
        "\"memory\": " + String(system_get_free_heap_size()) + ", "
        "\"rssi\": " + String(WiFi.RSSI()) + " }";
}

String getPinInfo(const device::Pin& pin) {
    return "{ "
        "\"name\": \"" + String(pin.name) + "\", "
        "\"value\": " + String(digitalRead(pin.number)) + " }";
}

}

String getContent(const String& path, const String& /*content*/) {
    if (!path.startsWith("/")) {
        return "";
    }

    size_t position = 0;
    String pinName = tools::nextToken(path, '/', position);
    if (pinName.length() == 0) {
        tools::Join pinData{", "};
        for (const device::Pin& pin : device::pins) {
            pinData.add(getPinInfo(pin));
        }
        return "{ \"device\": " + getDeviceInfo() + ", "
            "\"pins\": [ " + pinData.get() + " ] }";
    }

    auto pin = std::find_if(device::pins.begin(), device::pins.end(),
            [&pinName](const device::Pin& pin) { return pinName == pin.name; });
    if (pin == device::pins.end()) {
        return "";
    }

    String value = tools::nextToken(path, '/', position);
    if (value.length() != 0) {
        if (!pin->output) {
            return "";
        }
        int logicalValue = value.toInt();
        digitalWrite(pin->number, logicalValue);
    }
    return getPinInfo(*pin);
}
