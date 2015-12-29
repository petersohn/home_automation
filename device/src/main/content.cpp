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
    String type = (pin.output ? "output" : "input");
    return "{ "
        "\"type\": \"" + type + "\", "
        "\"name\": \"" + String(pin.name) + "\", "
        "\"value\": " + String(digitalRead(pin.number)) + " }";
}

String getModifiedPinsInfo() {
    constexpr int pressThreshold = 100;
    tools::Join result{", "};
    unsigned long now = millis();
    for (device::Pin& pin : device::pins) {
        if (pin.output) {
            continue;
        }
        bool status = digitalRead(pin.number);
        if (pin.status != status && (now - pin.lastSeen) > pressThreshold) {
            pin.status = status;
            pin.lastSeen = now;
            result.add(getPinInfo(pin));
        }
    }
    return result.get();
}

} // unnamed namespace

String getFullStatus(const String& type) {
    tools::Join pinData{", "};
    for (const device::Pin& pin : device::pins) {
        pinData.add(getPinInfo(pin));
    }
    String result = "{ \"device\": " + getDeviceInfo() + ", "
        "\"pins\": [ " + pinData.get() + " ]";
    if (type.length() != 0) {
        result += ", \"type\": \"" + type + "\"";
    }
    return result + " }";
}

String getContent(const String& path, const String& /*content*/) {
    if (!path.startsWith("/")) {
        return "";
    }

    size_t position = 0;
    String pinName = tools::nextToken(path, '/', position);
    if (pinName.length() == 0) {
        return getFullStatus("");
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

String getModifiedPinsContent(const String& type) {
    String modifiedPins = getModifiedPinsInfo();
    if (modifiedPins.length() != 0) {
        return "{ \"device\": " + getDeviceInfo() + ", "
            "\"pins\": [ " + modifiedPins + " ],"
            "\"type\": \"" + type + "\" }";
    } else {
        return {};
    }
}

