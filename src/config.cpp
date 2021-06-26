#include "config.hpp"

#include "ArduinoJson.hpp"
#include "client.hpp"
#include "CounterInterface.hpp"
#include "Cover.hpp"
#include "DallasTemperatureSensor.hpp"
#include "debug.hpp"
#include "DhtSensor.hpp"
#include "AnalogSensor.hpp"
#include "GpioInput.hpp"
#include "GpioOutput.hpp"
#include "KeepaliveInterface.hpp"
#include "MqttInterface.hpp"
#include "PublishAction.hpp"
#include "PowerSupplyInterface.hpp"$
#include "SensorInterface.hpp"
#include "common/CommandAction.hpp"
#include "operation/OperationParser.hpp"
#include "tools/collection.hpp"

#include <FS.h>

#include <algorithm>
#include <memory>

namespace {

struct ParsedData {
    std::unique_ptr<DynamicJsonBuffer> buffer{new DynamicJsonBuffer{512}};
    JsonObject* root = nullptr;
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
    result.root = &result.buffer->parseObject(f);
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

template<typename T>
bool getRequiredValue(const JsonObject& data, const char* name, T& value) {
    JsonVariant rawValue = data[name];
    if (rawValue.is<T>()) {
        value = rawValue.as<T>();
        return true;
    }
    debugln("Invalid " + std::string(name) + ": " + rawValue.as<std::string>());
    return false;
}

bool getPin(const JsonObject& data, int& value) {
    return getRequiredValue(data, "pin", value);
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

std::vector<std::string> getPulse(const JsonObject& data) {
    auto pulse = data.get<JsonVariant>("pulse");
    if (!pulse.success()) {
        return {};
    }

    auto& array = pulse.as<JsonArray>();
    if (array == JsonArray::invalid()) {
        return {pulse.as<std::string>()};
    }

    std::vector<std::string> result;
    result.reserve(array.size());
    for (const JsonVariant& value : array) {
        result.push_back(value.as<std::string>());
    }

    return result;
}

std::unique_ptr<Interface> createSensorInterface(const JsonObject& data,
        std::unique_ptr<Sensor>&& sensor) {
    return std::unique_ptr<Interface>{new SensorInterface{
            std::move(sensor), data.get<std::string>("name"),
            getInterval(data), getOffset(data), getPulse(data)}};
}

GpioInput::CycleType getCycleType(const std::string& value) {
    if (value == "none") {
        return GpioInput::CycleType::none;
    }
    if (value == "multi") {
        return GpioInput::CycleType::multi;
    }

    return GpioInput::CycleType::single;
}

std::unique_ptr<Interface> parseInterface(const JsonObject& data) {
    std::string type = data.get<std::string>("type");
    if (type == "input") {
        int pin = 0;
        return getPin(data, pin)
                ?  std::unique_ptr<Interface>{new GpioInput{
                        pin, getCycleType(data.get<std::string>("cycle"))}}
                : nullptr;
    } else if (type == "output") {
        int pin = 0;
        return getPin(data, pin)
                ?  std::unique_ptr<Interface>{
                        new GpioOutput{pin, data["default"], data["invert"]}}
                : nullptr;
    } else if (type == "analog") {
        return createSensorInterface(data,
                std::unique_ptr<Sensor>(new AnalogSensor{}));
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
        size_t devices = data.get<size_t>("devices");
        if (devices == 0) {
            devices = 1;
        }
        return getPin(data, pin)
                ?  createSensorInterface(data, std::unique_ptr<Sensor>(
                        new DallasTemperatureSensor{pin, devices}))
                : nullptr;
    } else if (type == "counter") {
        int pin = 0;
        return getPin(data, pin)
                ?  std::unique_ptr<Interface>(new CounterInterface{
                        data.get<std::string>("name"),
                        pin, getJsonWithDefault(data["bounceTime"], 0),
                        getJsonWithDefault(data["multiplier"], 1.0f),
                        getInterval(data), getOffset(data), getPulse(data)})
                : nullptr;
    } else if (type == "mqtt") {
        std::string topic = data["topic"];
        return topic.length() != 0
               ? std::unique_ptr<Interface>(new MqttInterface{topic})
               : nullptr;
    } else if (type == "keepalive") {
        int pin = 0;
        return getPin(data, pin)
                ?  std::unique_ptr<Interface>{new KeepaliveInterface{
                        pin, getJsonWithDefault(data["interval"], 10000),
                        getJsonWithDefault(data["resetInterval"], 10)}}
                : nullptr;
    } else if (type == "powerSupply") {
        int powerSwitchPin = 0;
        int resetSwitchPin = 0;
        int powerCheckPin = 0;
        return (getRequiredValue(data, "powerSwitchPin", powerSwitchPin)
                && getRequiredValue(data, "resetSwitchPin", resetSwitchPin)
                && getRequiredValue(data, "powerCheckPin", powerCheckPin))
                ?  std::unique_ptr<Interface>{new PowerSupplyInterface{
                        powerSwitchPin, resetSwitchPin, powerCheckPin,
                        getJsonWithDefault(data["pushTime"], 200),
                        getJsonWithDefault(data["forceOffTime"], 6000),
                        getJsonWithDefault(data["checkTime"], 60000),
                        getJsonWithDefault(data["initialState"], "")
                }}
                : nullptr;
    } else if (type == "cover") {
        int upMovementPin = 0;
        int downMovementPin = 0;
        int upPin = 0;
        int downPin = 0;
        return (getRequiredValue(data, "upMovementPin", upMovementPin)
                && getRequiredValue(data, "downMovementPin", downMovementPin)
                && getRequiredValue(data, "upPin", upPin)
                && getRequiredValue(data, "downPin", downPin))
                ?  std::unique_ptr<Interface>{new Cover{
                        upMovementPin, downMovementPin, upPin, downPin,
                        getJsonWithDefault(data["invertInput"], false),
                        getJsonWithDefault(data["invertOutput"], false),
                }}
                : nullptr;
    } else {
        debugln(std::string("Invalid interface type: ") + type);
        return {};
    }
}

void parseInterfaces(const JsonObject& data,
        std::vector<std::unique_ptr<InterfaceConfig>>& result) {
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

        result.emplace_back(new InterfaceConfig);
        InterfaceConfig& interfaceConfig = *result.back();

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

std::pair<std::unique_ptr<Action>, std::unordered_set<InterfaceConfig*>>
parseAction(JsonObject& data,
        InterfaceConfig* defaultInterface,
        std::vector<std::unique_ptr<InterfaceConfig>>& interfaces) {
    auto type = data.get<std::string>("type");
    operation::Parser operationParser{interfaces, defaultInterface};
    std::unique_ptr<Action> result;
    if (type == "publish") {
        std::string topic = getMandatoryArgument(data, "topic");
        if (topic.empty()) {
            return {};
        }
        if (!data["payload"].success() && !data["template"].success()) {
            data.set("template", "%1");
        }
        result = std::unique_ptr<Action>(new PublishAction{topic,
                operationParser.parse(data, "payload", "template"),
                data.get<bool>("retain"),
                data.get<unsigned>("minimumSendInterval")});
    } else if (type == "command") {
        auto target = findInterface(interfaces, data["target"]);
        if (!target) {
            return {};
        }

        result = std::unique_ptr<Action>(new CommandAction{*target->interface,
                operationParser.parse(data, "command", "template")});
    } else {
        debugln("Invalid action type: " + type);
    }

    return {std::move(result), std::move(operationParser).getUsedInterfaces()};
}

void parseActions(JsonObject& data,
        std::vector<std::unique_ptr<InterfaceConfig>>& interfaces) {
    const JsonArray& actions = data["actions"];
    if (actions == JsonArray::invalid()) {
        debugln("Could not parse actions.");
        return;
    }

    for (JsonObject& action : actions) {
        std::string interfaceName = action["interface"];
        auto defaultInterface = findInterface(interfaces, action["interface"]);

        auto parseResult = parseAction(action, defaultInterface, interfaces);
        std::shared_ptr<Action> parsedAction = std::move(parseResult.first);
        auto&& usedInterfaces = parseResult.second;
        if (!parsedAction) {
            debugln("Invalid action configuration.");
            continue;
        }

        if (defaultInterface) {
            usedInterfaces.insert(defaultInterface);
        }
        for (auto& interface : usedInterfaces) {
            interface->actions.push_back(parsedAction);
        }
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

    PARSE(*data.root, result, debugTopic);
    if (!result.debugTopic.empty()) {
        mqttDebugger.reset(new MqttDebugger(result.debugTopic));
    }

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
    deviceConfig.debug = true;
    Serial.begin(115200);
    Serial.println("Filesystem");
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
        Serial.println(dir.fileName());
    }
    deviceConfig = readDeviceConfig("/device_config.json");
    globalConfig = readGlobalConfig("/global_config.json");
}
