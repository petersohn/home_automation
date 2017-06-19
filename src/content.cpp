#include "config.hpp"
#include "content.hpp"
#include "debug.hpp"
#include "http.hpp"
#include "string.hpp"

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
        "\"name\": \"" + String(deviceConfig.name) + "\", "
        "\"ip\": \"" + ipString + "\", "
        "\"port\": 80, "
        "\"memory\": " + String(system_get_free_heap_size()) + ", "
        "\"version\": 2, "
        "\"rssi\": " + String(WiFi.RSSI()) + " }";
}

String handleInterface(const InterfaceConfig& interface, const String& path) {
    auto result = interface.interface->answer(path);
    if (result.success) {
        return "{ "
            "\"name\": \"" + String(interface.name) + "\", "
            "\"value\": " + result.value + " }";
    }
    DEBUGLN(result.value);
    return "";
}

String getFullStatus() {
    tools::Join interfaceData{", "};
    for (const InterfaceConfig& pin : deviceConfig.interfaces) {
        interfaceData.add(handleInterface(pin, ""));
    }
    String result = "{ \"device\": " + getDeviceInfo() + ", "
        "\"pins\": [ " + interfaceData.get() + " ]";
    return result + " }";
}

} // unnamed namespace

String getContent(const String& method, const String& path,
        const String& content) {
    if (!path.startsWith("/")) {
        return "";
    }

    int interfaceNameEnd = path.indexOf('/', 1);
    if (interfaceNameEnd == -1) {
        interfaceNameEnd = path.length();
    }

    String interfaceName = path.substring(1, interfaceNameEnd);
    if (interfaceName.length() == 0) {
        return getFullStatus();
    }

    auto interface = std::find_if(
            deviceConfig.interfaces.begin(), deviceConfig.interfaces.end(),
            [&interfaceName](const InterfaceConfig& interface) {
                return interfaceName == interface.name;
            });
    if (interface == deviceConfig.interfaces.end()) {
        return "";
    }

    const String& value = (method == "POST") ? content
            : path.substring(interfaceNameEnd + 1);
    return handleInterface(*interface, value);
}
