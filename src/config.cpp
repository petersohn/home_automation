#include "config.hpp"

#include "collection.hpp"
#include "CommandAction.hpp"
#include "ConditionalAction.hpp"
#include "CounterInterface.hpp"
#include "DallasTemperatureSensor.hpp"
#include "debug.hpp"
#include "DhtSensor.hpp"
#include "GpioInterface.hpp"
#include "PublishAction.hpp"
#include "SensorInterface.hpp"

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

template<typename T>
T getJsonWithDefault(JsonVariant data, T defaultValue) {
    T result = data.as<T>();
    if (result == T{}) {
        return defaultValue;
    }
    return result;
}

const std::initializer_list<std::pair<const char*, int>> dhtTypes{
    {"", DHT22}, {"dht11", DHT11}, {"dht22", DHT22}, {"dht21", DHT21}
};

int getInterval(const JsonObject& data) {
    return getJsonWithDefault(data["interval"], 60) * 1000;
}

int getOffset(const JsonObject& data) {
    return getJsonWithDefault(data["offset"], 0) * 1000;
}

std::unique_ptr<Interface> createSensorInterface(const JsonObject& data,
        std::unique_ptr<Sensor>&& sensor) {
    return std::unique_ptr<Interface>{new SensorInterface{
            std::move(sensor), getInterval(data), getOffset(data)}};

}

std::unique_ptr<Interface> parseInterface(const JsonObject& data) {
    String type = data.get<String>("type");
    if (type == "input") {
        int pin = 0;
        return getPin(data, pin)
                ?  std::unique_ptr<Interface>{
                        new GpioInput{pin}}
                : nullptr;
    } else if (type == "output") {
        int pin = 0;
        return getPin(data, pin)
                ?  std::unique_ptr<Interface>{
                        new GpioOutput{pin, data["default"]}}
                : nullptr;
    } else if (type == "dht") {
        int pin = 0;
        auto type = tools::findValue(dhtTypes,
                data.get<String>("dhtType"));
        if (!type) {
            DEBUGLN("Invalid DHT type.");
            return nullptr;
        }
        return getPin(data, pin)
                ?  createSensorInterface(data,
                        std::unique_ptr<Sensor>(new DhtSensor{pin, *type}))
                : nullptr;
    } else if (type == "dallasTemperature") {
        int pin = 0;
        return getPin(data, pin)
                ?  createSensorInterface(data, std::unique_ptr<Sensor>(
                        new DallasTemperatureSensor{pin}))
                : nullptr;
    } else if (type == "counter") {
        int pin = 0;
        return getPin(data, pin)
                ?  std::unique_ptr<Interface>(new CounterInterface{
                        pin, getJsonWithDefault(data["multiplier"], 1.0f),
                        getInterval(data), getOffset(data)})
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

String getMandatoryArgument(const JsonObject& data, const char* name) {
    String result = data[name];
    if (result.length() == 0) {
        DEBUGLN(String(name) + " is mandatory.");
    }
    return result;
}

InterfaceConfig* findInterface(
        std::vector<InterfaceConfig>& interfaces, const char* name) {
    auto interface = std::find_if(interfaces.begin(), interfaces.end(),
            [&name](const InterfaceConfig& interface) {
                return interface.name == name;
            });
    if (interface == interfaces.end()) {
        DEBUGLN("Could not find interface: " + String(name));
        return nullptr;
    }
    return &*interface;
}

std::unique_ptr<Action> parseBareAction(const JsonObject& data,
        std::vector<InterfaceConfig>& interfaces) {
    String type = data.get<String>("type");
    if (type == "publish") {
        String topic = getMandatoryArgument(data, "topic");
        if (topic.length() == 0) {
            return {};
        }
        return std::unique_ptr<Action>(new PublishAction{topic,
                data.get<String>("template"), data.get<bool>("retain")});
    } else if (type == "command") {
        String command = getMandatoryArgument(data, "command");
        if (command.length() == 0) {
            return {};
        }

        auto target = findInterface(interfaces, data["target"]);
        if (!target) {
            return {};
        }

        return std::unique_ptr<Action>(new CommandAction{*target->interface,
                command});
    } else {
        DEBUGLN("Invalid action type: " + type);
        return {};
    }
}

std::unique_ptr<Action> parseAction(const JsonObject& data,
        std::vector<InterfaceConfig>& interfaces) {
    String value = data["value"];
    if (value.length() != 0) {
        return std::unique_ptr<Action>{new ConditionalAction{
                value, parseBareAction(data, interfaces)}};
    }
    return parseBareAction(data, interfaces);
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
        auto interface = findInterface(interfaces, action["interface"]);
        if (!interface) {
            continue;
        }

        auto parsedAction = parseAction(action, interfaces);
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

    PARSE(*data.root, result, debug, bool);
    if (result.debug) {
        deviceConfig.debug = result.debug;
        Serial.begin(115200);
        DEBUGLN();
        DEBUGLN("Starting up...");
    }
    PARSE(*data.root, result, name, String);
    PARSE(*data.root, result, availabilityTopic, String);

    parseInterfaces(*data.root, result.interfaces);
    parseActions(*data.root, result.interfaces);

    return result;
}

} // unnamed namespace

GlobalConfig globalConfig;
DeviceConfig deviceConfig;

void initConfig() {
    SPIFFS.begin();
    deviceConfig = readDeviceConfig("/device_config.json");
    globalConfig = readGlobalConfig("/global_config.json");
}
