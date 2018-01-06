#include "config.hpp"

#include "ArduinoJson.hpp"
#include "client.hpp"
#include "CounterInterface.hpp"
#include "DallasTemperatureSensor.hpp"
#include "debug.hpp"
#include "DhtSensor.hpp"
#include "GpioInput.hpp"
#include "GpioOutput.hpp"
#include "MqttInterface.hpp"
#include "PublishAction.hpp"
#include "SensorInterface.hpp"
#include "common/CommandAction.hpp"
#include "common/ConditionalAction.hpp"
#include "tools/collection.hpp"

#include <FS.h>

#include <algorithm>

namespace {

struct ParsedData {
    DynamicJsonBuffer buffer{512};
    const JsonObject* root = nullptr;
};

template<typename T>
void parseTo(const JsonObject& jsonObject, T& target, const char* name) {
    auto value = jsonObject.get<JsonVariant>(name);
    if (value.is<T>()) {
        target = value.as<T>();
    }
}

void parseTo(const JsonObject& jsonObject, std::string& target,
        const char* name) {
    auto value = jsonObject.get<const char*>(name);
    if (value) {
        target = value;
    }
}

#define PARSE(from, to, name) parseTo((from), (to).name, #name)

ParsedData parseFile(const char* filename) {
    ParsedData result;
    File f = SPIFFS.open(filename, "r");
    if (!f) {
        debugln(std::string("Could not open file: ") + filename);
        return result;
    }
    result.root = &result.buffer.parseObject(f);
    if (*result.root == JsonObject::invalid()) {
        debugln(std::string("Could not parse JSON file: ") + filename);
        result.root = nullptr;
    }

    return result;
}

ServerConfig parseServerConfig(const JsonObject& data) {
    ServerConfig result;
    PARSE(data, result, address);
    PARSE(data, result, port);
    PARSE(data, result, username);
    PARSE(data, result, password);
    return result;
}

ServerConfig parseSingleServerConfig(const JsonObject& data) {
    ServerConfig result;
    result.address = data.get<std::string>("serverAddress");
    result.port = data.get<uint16_t>("serverPort");
    result.username = data.get<std::string>("serverUsername");
    result.password = data.get<std::string>("serverPassword");
    return result;
}

GlobalConfig readGlobalConfig(const char* filename) {
    GlobalConfig result;
    ParsedData data = parseFile(filename);
    if (!data.root) {
        return result;
    }

    PARSE(*data.root, result, wifiSSID);
    PARSE(*data.root, result, wifiPassword);

    JsonArray& servers = data.root->get<JsonVariant>("servers");
    if (servers == JsonArray::invalid()) {
        debugln("No servers config. "
                "Attempting old-style single-server config.");
        result.servers.push_back(parseSingleServerConfig(*data.root));
    } else {
        for (auto server : servers) {
            result.servers.push_back(parseServerConfig(
                    server.as<JsonObject>()));
        }
    }

    return result;
}

bool getPin(const JsonObject& data, int& value) {
    JsonVariant rawValue = data["pin"];
    if (rawValue.is<int>()) {
        value = rawValue.as<int>();
        return true;
    }
    debugln("Invalid pin: " + rawValue.as<std::string>());
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
    std::string type = data.get<std::string>("type");
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
                data.get<std::string>("dhtType"));
        if (!type) {
            debugln("Invalid DHT type.");
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
    } else if (type == "mqtt") {
        std::string topic = data["topic"];
        return topic.length() != 0
               ? std::unique_ptr<Interface>(new MqttInterface{topic})
               : nullptr;
    } else {
        debugln(std::string("Invalid interface type: ") + type);
        return {};
    }
}

void parseInterfaces(const JsonObject& data,
        std::vector<InterfaceConfig>& result) {
    const JsonArray& interfaces = data["interfaces"];
    if (interfaces == JsonArray::invalid()) {
        debugln("Could not parse interfaces.");
        return;
    }

    result.reserve(interfaces.size());
    for (const JsonObject& interface : interfaces) {
        if (interface == JsonObject::invalid()) {
            debugln("Interface configuration must be an array.");
            continue;
        }

        auto parsedInterface = parseInterface(interface);
        if (!parsedInterface) {
            debugln("Invalid interface configuration.");
            continue;
        }

        result.emplace_back();
        InterfaceConfig& interfaceConfig = result.back();

        PARSE(interface, interfaceConfig, name);

        std::string commandTopic = interface["commandTopic"];
        if (!commandTopic.empty()) {
            mqtt::subscribe(commandTopic,
                    [&interfaceConfig](const std::string& command) {
                        interfaceConfig.interface->execute(command);
                    });
        }

        interfaceConfig.interface = std::move(parsedInterface);
    }
}

std::string getMandatoryArgument(const JsonObject& data, const char* name) {
    std::string result = data[name];
    if (result.length() == 0) {
        debugln(std::string(name) + " is mandatory.");
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
        debugln("Could not find interface: " + std::string(name));
        return nullptr;
    }
    return &*interface;
}

std::unique_ptr<Action> parseBareAction(const JsonObject& data,
        std::vector<InterfaceConfig>& interfaces) {
    auto type = data.get<std::string>("type");
    if (type == "publish") {
        std::string topic = getMandatoryArgument(data, "topic");
        if (topic.length() == 0) {
            return {};
        }
        return std::unique_ptr<Action>(new PublishAction{topic,
                data.get<std::string>("template"), data.get<bool>("retain")});
    } else if (type == "command") {
        std::string command = getMandatoryArgument(data, "command");
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
        debugln("Invalid action type: " + type);
        return {};
    }
}

std::unique_ptr<Action> parseAction(const JsonObject& data,
        std::vector<InterfaceConfig>& interfaces) {
    std::string value = data["value"];
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
        debugln("Could not parse actions.");
        return;
    }

    for (const JsonObject& action : actions) {
        std::string interfaceName = action["interface"];
        auto interface = findInterface(interfaces, action["interface"]);
        if (!interface) {
            continue;
        }

        auto parsedAction = parseAction(action, interfaces);
        if (!parsedAction) {
            debugln("Invalid action configuration.");
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

    PARSE(*data.root, result, debug);
    if (result.debug) {
        deviceConfig.debug = result.debug;
        Serial.begin(115200);
    }

    PARSE(*data.root, result, debugPort);
    if (result.debugPort != 0) {
        wifiDebugger.reset(new WiFiDebugger(result.debugPort));
    }

    debugln();
    debugln("Starting up...");
    debug("Debug port = ");
    debugln(result.debugPort);

    PARSE(*data.root, result, name);
    PARSE(*data.root, result, availabilityTopic);

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
