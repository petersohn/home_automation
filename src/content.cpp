#include "config.hpp"
#include "content.hpp"
#include "debug.hpp"
#include "http.hpp"
#include "string.hpp"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

#include <algorithm>

extern "C" {
#include "user_interface.h"
}

namespace {

JsonObject& getDeviceInfo(DynamicJsonBuffer& buffer) {
    IPAddress ip = WiFi.localIP();
    String ipString = String(ip[0]) + '.' + String(ip[1]) + '.' +
            String(ip[2]) + '.' + String(ip[3]);
    JsonObject& result = buffer.createObject();
    result["name"] = deviceConfig.name;
    result["ip"] = ipString;
    result["port"] = 80;
    result["memory"] = system_get_free_heap_size();
    result["version"] = 2;
    result["uptime"] = millis();
    result["rssi"] = WiFi.RSSI();
    return result;
}

JsonObject* handleInterface(DynamicJsonBuffer& buffer,
        const InterfaceConfig& interface, const String& path) {
    auto answer = interface.interface->answer(path);
    if (answer.success) {
        JsonObject& result = buffer.createObject();
        result["name"] = interface.name;
        result["value"] = answer.value;
        return &result;
    }
    DEBUGLN(answer.value);
    return nullptr;
}

JsonObject& getFullStatus(DynamicJsonBuffer& buffer) {
    JsonObject& result = buffer.createObject();
    result["device"] = getDeviceInfo(buffer);

    JsonArray& interfaceData = buffer.createArray();
    result["interfaces"] = interfaceData;

    for (const InterfaceConfig& pin : deviceConfig.interfaces) {
        JsonObject* object = handleInterface(buffer, pin, "");
        if (object) {
            interfaceData.add(*object);
        }
    }

    return result;
}

JsonObject* doGetContent(DynamicJsonBuffer& buffer,
        const String& method, const String& path,
        const String& content) {
    if (!path.startsWith("/")) {
        return nullptr;
    }

    int interfaceNameEnd = path.indexOf('/', 1);
    if (interfaceNameEnd == -1) {
        interfaceNameEnd = path.length();
    }

    String interfaceName = path.substring(1, interfaceNameEnd);
    if (interfaceName.length() == 0) {
        return &getFullStatus(buffer);
    }

    auto interface = std::find_if(
            deviceConfig.interfaces.begin(), deviceConfig.interfaces.end(),
            [&interfaceName](const InterfaceConfig& interface) {
                return interfaceName == interface.name;
            });
    if (interface == deviceConfig.interfaces.end()) {
        return nullptr;
    }

    const String& value = (method == "POST") ? content
            : path.substring(interfaceNameEnd + 1);
    return handleInterface(buffer, *interface, value);
}

} // unnamed namespace

String getContent(const String& method, const String& path,
        const String& content) {
    DynamicJsonBuffer buffer{512};
    JsonObject* result = doGetContent(buffer, method, path, content);

    if (!result) {
        return "";
    }

    String output;
    result->printTo(output);
    return output;
}
