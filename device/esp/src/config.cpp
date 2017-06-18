#include "config.hpp"
#include "Interface.hpp"

#include <ArduinoJson.h>
#include <FS.h>

#define PARSE(from, to, name, type) (to).name = (from).get<type>(#name)

namespace {

struct ParsedData {
    DynamicJsonBuffer buffer{512};
    const JsonObject* root = nullptr;
};

ParsedData parseFile(const char* filename) {
    ParsedData result;
    File f = SPIFFS.open(filename, "r");
    if (!f) {
        Serial.println(String("Could not open file: ") + filename);
        return result;
    }
    result.root = &result.buffer.parseObject(f);
    if (*result.root == JsonObject::invalid()) {
        Serial.println(String("Could not parse JSON file: ") + filename);
        result.root = nullptr;
    }

    return result;
}

GlobalConfig readGlobalConfig(const char* filename) {
    GlobalConfig result;
    ParsedData data = parseFile(filename);
    if (!data.root) {
        return result;
    }

    PARSE(*data.root, result, wifiSSID, String);
    PARSE(*data.root, result, wifiPassword, String);
    PARSE(*data.root, result, serverAddress, String);
    PARSE(*data.root, result, serverPort, int);
    PARSE(*data.root, result, serverPassword, String);

    return result;
}

std::unique_ptr<Interface> parseInterface(const JsonObject& data) {
    String type = data.get<String>("type");
    if (type == "input") {
        return std::unique_ptr<Interface>{new GpioInput(data.get<int>("pin"))};
    } else if (type == "output") {
        return std::unique_ptr<Interface>{new GpioOutput(data.get<int>("pin"))};
    } else {
        Serial.println(String("Invalid interface type: ") + type);
        return {};
    }
}

DeviceConfig readDeviceConfig(const char* filename) {
    DeviceConfig result;
    ParsedData data = parseFile(filename);
    if (!data.root) {
        return result;
    }

    PARSE(*data.root, result, name, String);
    PARSE(*data.root, result, debug, bool);
    const JsonArray& interfaces = (*data.root)["interfaces"];
    if (interfaces == JsonArray::invalid()) {
        Serial.println("Could not parse interfaces.");
        return result;
    }

    result.interfaces.reserve(interfaces.size());
    for (const JsonObject& interface : interfaces) {
        if (interface == JsonObject::invalid()) {
            Serial.println("Could not parse interface.");
            continue;
        }

        result.interfaces.emplace_back();
        InterfaceConfig& interfaceConfig = result.interfaces.back();
        PARSE(interface, interfaceConfig, name, String);
    }

    return result;
}

} // unnamed namespace

GlobalConfig globalConfig;
DeviceConfig deviceConfig;

void initConfig() {
    globalConfig = readGlobalConfig("global_config.json");
    deviceConfig = readDeviceConfig("device_config.json");
}

std::vector<InterfaceConfig*> getModifiedInterfaces() {
    std::vector<InterfaceConfig*> result;
    for (InterfaceConfig& interface : deviceConfig.interfaces) {
        String value = interface.interface->get();
        if (interface.lastValue != value) {
            interface.lastValue = value;
            result.push_back(&interface);
        }
    }
    return result;
}
