#include "config.hpp"
#include "debug.hpp"
#include "GpioInterface.hpp"
#include "PublishAction.hpp"

#include <ArduinoJson.h>
#include <FS.h>

#include <algorithm>

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
        DEBUGLN(String("Could not open file: ") + filename);
        return result;
    }
    result.root = &result.buffer.parseObject(f);
    if (*result.root == JsonObject::invalid()) {
        DEBUGLN(String("Could not parse JSON file: ") + filename);
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
    PARSE(*data.root, result, serverUsername, String);
    PARSE(*data.root, result, serverPassword, String);

    return result;
}

bool getPin(const JsonObject& data, int& value) {
    JsonVariant rawValue = data["pin"];
    if (rawValue.is<int>()) {
        value = rawValue.as<int>();
        return true;
    }
    DEBUGLN("Invalid pin: " + rawValue.as<String>());
    return false;
}

std::unique_ptr<Interface> parseInterface(const JsonObject& data) {
    String type = data.get<String>("type");
    if (type == "input") {
        int pin = 0;
        return getPin(data, pin)
                ?  std::unique_ptr<Interface>{
                        new GpioInput(data.get<int>("pin"))}
                : nullptr;
    } else if (type == "output") {
        int pin = 0;
        return getPin(data, pin)
                ?  std::unique_ptr<Interface>{
                        new GpioOutput(data.get<int>("pin"))}
                : nullptr;
    } else {
        DEBUGLN(String("Invalid interface type: ") + type);
        return {};
    }
}

void parseInterfaces(const JsonObject& data,
        std::vector<InterfaceConfig>& result) {
    const JsonArray& interfaces = data["interfaces"];
    if (interfaces == JsonArray::invalid()) {
        DEBUGLN("Could not parse interfaces.");
        return;
    }

    result.reserve(interfaces.size());
    for (const JsonObject& interface : interfaces) {
        if (interface == JsonObject::invalid()) {
            DEBUGLN("Interface configuration must be an array.");
            continue;
        }

        auto parsedInterface = parseInterface(interface);
        if (!parsedInterface) {
            DEBUGLN("Invalid interface configuration.");
            continue;
        }

        result.emplace_back();
        InterfaceConfig& interfaceConfig = result.back();
        PARSE(interface, interfaceConfig, name, String);
        PARSE(interface, interfaceConfig, commandTopic, String);
        interfaceConfig.interface = std::move(parsedInterface);
    }
}

std::unique_ptr<Action> parseAction(const JsonObject& data) {
    String type = data.get<String>("type");
    if (type == "publish") {
        String topic = data["topic"];
        if (topic.length() == 0) {
            DEBUGLN("Topic must be given.");
            return {};
        }
        return std::unique_ptr<Action>(new PublishAction{
                topic, data.get<bool>("retain")});
    } else {
        DEBUGLN("Invalid action type: " + type);
        return {};
    }
}

void parseActions(const JsonObject& data,
        std::vector<InterfaceConfig>& interfaces) {
    const JsonArray& actions = data["actions"];
    if (actions == JsonArray::invalid()) {
        DEBUGLN("Could not parse actions.");
        return;
    }

    for (const JsonObject& action : actions) {
        String interfaceName = action["interface"];
        auto interface = std::find_if(interfaces.begin(), interfaces.end(),
                [&interfaceName](const InterfaceConfig& interface) {
                    return interface.name == interfaceName;
                });
        if (interface == interfaces.end()) {
            DEBUGLN("Could not find interface: " + interfaceName);
            continue;
        }

        auto parsedAction = parseAction(action);
        if (!parsedAction) {
            DEBUGLN("Invalid action configuration.");
            continue;
        }

        interface->actions.push_back(std::move(parsedAction));
    }
}

DeviceConfig readDeviceConfig(const char* filename) {
    DeviceConfig result;
    ParsedData data = parseFile(filename);
    if (!data.root) {
        return result;
    }

    PARSE(*data.root, result, name, String);
    PARSE(*data.root, result, availabilityTopic, String);
    PARSE(*data.root, result, debug, bool);

    parseInterfaces(*data.root, result.interfaces);
    parseActions(*data.root, result.interfaces);

    return result;
}

} // unnamed namespace

GlobalConfig globalConfig;
DeviceConfig deviceConfig;

void initConfig() {
    SPIFFS.begin();
    globalConfig = readGlobalConfig("/global_config.json");
    deviceConfig = readDeviceConfig("/device_config.json");
}
