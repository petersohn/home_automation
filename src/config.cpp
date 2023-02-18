#include "config.hpp"

#include "common/ArduinoJson.hpp"
#include "client.hpp"
#include "CounterInterface.hpp"
#include "Cover.hpp"
#include "DallasTemperatureSensor.hpp"
#include "DhtSensor.hpp"
#include "AnalogSensor.hpp"
#include "GpioInput.hpp"
#include "GpioOutput.hpp"
#include "Hlw8012Interface.hpp"
#include "HM3301Sensor.hpp"
#include "KeepaliveInterface.hpp"
#include "MqttInterface.hpp"
#include "PublishAction.hpp"
#include "PowerSupplyInterface.hpp"$
#include "SensorInterface.hpp"
#include "StatusInterface.hpp"
#include "DebugStream.hpp"
#include "JsonParser.hpp"
#include "common/CommandAction.hpp"
#include "operation/OperationParser.hpp"
#include "tools/collection.hpp"

#include <FS.h>

#include <algorithm>
#include <memory>

using namespace ArduinoJson;

namespace {


class ConfigParser {
public:
    ConfigParser(std::ostream& debug, DebugStreambuf& debugStream, EspApi& esp,
            Rtc& rtc, MqttClient& mqttClient)
        : debug(debug)
        , debugStream(debugStream)
        , esp(esp)
        , rtc(rtc)
        , mqttClient(mqttClient)
        , jsonParser(debug) {}

    void parse() {
        SPIFFS.begin();

        deviceConfig = readDeviceConfig("/device_config.json");
        globalConfig = readGlobalConfig("/global_config.json");
    }

private:
    std::ostream& debug;
    DebugStreambuf& debugStream;
    EspApi& esp;
    Rtc& rtc;
    MqttClient& mqttClient;

    JsonParser jsonParser;

    ServerConfig parseServerConfig(const JsonObject& data) {
        ServerConfig result;
        PARSE(jsonParser, data, result, address);
        PARSE(jsonParser, data, result, port);
        PARSE(jsonParser, data, result, username);
        PARSE(jsonParser, data, result, password);
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
        ParsedData data = jsonParser.parseFile(filename);
        if (!data.root) {
            return result;
        }

        PARSE(jsonParser, *data.root, result, wifiSSID);
        PARSE(jsonParser, *data.root, result, wifiPassword);

        JsonArray& servers = data.root->get<JsonVariant>("servers");
        if (servers == JsonArray::invalid()) {
            debug << "No servers config. "
                    "Attempting old-style single-server config." << std::endl;
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
        debug << "Invalid " << name << ": " << rawValue.as<std::string>()
            << std::endl;
        return false;
    }

    bool getPin(const JsonObject& data, uint8_t& value) {
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
        return std::make_unique<SensorInterface>(debug, esp,
                std::move(sensor), data.get<std::string>("name"),
                getInterval(data), getOffset(data), getPulse(data));
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
            uint8_t pin = 0;
            return getPin(data, pin)
                    ?  std::make_unique<GpioInput>(debug, esp,
                            pin, getCycleType(data.get<std::string>("cycle")))
                    : nullptr;
        } else if (type == "output") {
            uint8_t pin = 0;
            return getPin(data, pin)
                    ?  std::make_unique<GpioOutput>(debug, esp, rtc,
                            pin, data["default"], data["invert"])
                    : nullptr;
        } else if (type == "analog") {
            return createSensorInterface(data,
                    std::make_unique<AnalogSensor>());
        } else if (type == "dht") {
            uint8_t pin = 0;
            auto type = tools::findValue(dhtTypes,
                    data.get<std::string>("dhtType"));
            if (!type) {
                debug << "Invalid DHT type." << std::endl;
                return nullptr;
            }
            return getPin(data, pin)
                    ?  createSensorInterface(data,
                            std::make_unique<DhtSensor>(debug, pin, *type))
                    : nullptr;
        } else if (type == "dallasTemperature") {
            uint8_t pin = 0;
            size_t devices = data.get<size_t>("devices");
            if (devices == 0) {
                devices = 1;
            }
            return getPin(data, pin)
                    ?  createSensorInterface(data,
                        std::make_unique<DallasTemperatureSensor>(
                            debug, pin, devices))
                    : nullptr;
        } else if (type == "hm3301") {
            int sda = 0;
            int scl = 0;
            return (getRequiredValue(data, "sda", sda)
                    && getRequiredValue(data, "scl", scl))
                    ?  createSensorInterface(data,
                        std::make_unique<HM3301Sensor>(debug, sda, scl))
                    : nullptr;
        } else if (type == "counter") {
            uint8_t pin = 0;
            return getPin(data, pin)
                    ?  std::make_unique<CounterInterface>(debug, esp,
                            data.get<std::string>("name"),
                            pin, getJsonWithDefault(data["bounceTime"], 0),
                            getJsonWithDefault(data["multiplier"], 1.0f),
                            getInterval(data), getOffset(data), getPulse(data))
                    : nullptr;
        } else if (type == "mqtt") {
            std::string topic = data["topic"];
            return topic.length() != 0
                   ? std::make_unique<MqttInterface>(mqttClient, topic)
                   : nullptr;
        } else if (type == "keepalive") {
            uint8_t pin = 0;
            return getPin(data, pin)
                    ?  std::make_unique<KeepaliveInterface>(esp,
                            pin, getJsonWithDefault(data["interval"], 10000),
                            getJsonWithDefault(data["resetInterval"], 10))
                    : nullptr;
        } else if (type == "powerSupply") {
            uint8_t powerSwitchPin = 0;
            uint8_t resetSwitchPin = 0;
            uint8_t powerCheckPin = 0;
            return (getRequiredValue(data, "powerSwitchPin", powerSwitchPin)
                    && getRequiredValue(data, "resetSwitchPin", resetSwitchPin)
                    && getRequiredValue(data, "powerCheckPin", powerCheckPin))
                    ?  std::make_unique<PowerSupplyInterface>(debug, esp,
                            powerSwitchPin, resetSwitchPin, powerCheckPin,
                            getJsonWithDefault(data["pushTime"], 200),
                            getJsonWithDefault(data["forceOffTime"], 6000),
                            getJsonWithDefault(data["checkTime"], 60000),
                            getJsonWithDefault(data["initialState"], ""))
                    : nullptr;
        } else if (type == "cover") {
            uint8_t upMovementPin = 0;
            uint8_t downMovementPin = 0;
            uint8_t upPin = 0;
            uint8_t downPin = 0;
            return (getRequiredValue(data, "upMovementPin", upMovementPin)
                    && getRequiredValue(data, "downMovementPin", downMovementPin)
                    && getRequiredValue(data, "upPin", upPin)
                    && getRequiredValue(data, "downPin", downPin))
                    ?  std::make_unique<Cover>(debug, esp, rtc,
                            upMovementPin, downMovementPin, upPin, downPin,
                            getJsonWithDefault(data["invertInput"], false),
                            getJsonWithDefault(data["invertOutput"], false),
                            getJsonWithDefault(data["closedPosition"], 0))
                    : nullptr;
        } else if (type == "hlw8012") {
            uint8_t powerPin = 0;
            return (getRequiredValue(data, "powerPin", powerPin))
                    ?  std::make_unique<Hlw8012Interface>(debug, esp,
                            data.get<std::string>("name"), getInterval(data),
                            getOffset(data), powerPin)
                    : nullptr;
        } else if (type == "status") {
            return std::make_unique<StatusInterface>(mqttClient);
        } else {
            debug << "Invalid interface type: " << type << std::endl;
            return {};
        }
    }

    void parseInterfaces(const JsonObject& data,
            std::vector<std::unique_ptr<InterfaceConfig>>& result) {
        const JsonArray& interfaces = data["interfaces"];
        if (interfaces == JsonArray::invalid()) {
            debug << "Could not parse interfaces." << std::endl;
            return;
        }

        result.reserve(interfaces.size());
        for (const JsonObject& interface : interfaces) {
            if (interface == JsonObject::invalid()) {
                debug << "Interface configuration must be an array."
                    << std::endl;
                continue;
            }

            auto parsedInterface = parseInterface(interface);
            if (!parsedInterface) {
                debug << "Invalid interface configuration." << std::endl;
                continue;
            }

            result.emplace_back(std::make_unique<InterfaceConfig>());
            InterfaceConfig& interfaceConfig = *result.back();

            PARSE(jsonParser, interface, interfaceConfig, name);

            std::string commandTopic = interface["commandTopic"];
            if (!commandTopic.empty()) {
                mqttClient.subscribe(commandTopic,
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
            debug << std::string(name) + " is mandatory." << std::endl;
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
            result = std::make_unique<PublishAction>(debug, mqttClient, topic,
                    operationParser.parse(data, "payload", "template"),
                    data.get<bool>("retain"),
                    data.get<unsigned>("minimumSendInterval"));
        } else if (type == "command") {
            auto target = findInterface(interfaces, data["target"]);
            if (!target) {
                return {};
            }

            result = std::make_unique<CommandAction>(*target->interface,
                    operationParser.parse(data, "command", "template"));
        } else {
            debug << "Invalid action type: " + type << std::endl;
        }

        return {std::move(result), std::move(operationParser).getUsedInterfaces()};
    }

    void parseActions(JsonObject& data,
            std::vector<std::unique_ptr<InterfaceConfig>>& interfaces) {
        const JsonArray& actions = data["actions"];
        if (actions == JsonArray::invalid()) {
            debug << "Could not parse actions." << std::endl;
            return;
        }

        for (JsonObject& action : actions) {
            std::string interfaceName = action["interface"];
            auto defaultInterface = findInterface(interfaces, action["interface"]);

            auto parseResult = parseAction(action, defaultInterface, interfaces);
            std::shared_ptr<Action> parsedAction = std::move(parseResult.first);
            auto&& usedInterfaces = parseResult.second;
            if (!parsedAction) {
                debug << "Invalid action configuration." << std::endl;
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
        ParsedData data = jsonParser.parseFile(filename);
        if (!data.root) {
            return result;
        }

        bool isDebug = false;
        jsonParser.parseTo(*data.root, isDebug, "debug");
        if (isDebug) {
            Serial.begin(115200);
            result.debug = std::make_unique<PrintStreambuf>(Serial);
            debugStream.add(result.debug.get());
        }

        PARSE(jsonParser, *data.root, result, debugPort);
        PARSE(jsonParser, *data.root, result, debugTopic);
        PARSE(jsonParser, *data.root, result, resetPin);
        if (result.resetPin <= 16) {
            esp.pinMode(result.resetPin, GpioMode::input);
        }

        debug << "\nStarting up...\n" << "Debug port = "
            << result.debugPort << ", reset pin = "
            << static_cast<int>(result.resetPin) << std::endl;

        PARSE(jsonParser, *data.root, result, name);
        PARSE(jsonParser, *data.root, result, availabilityTopic);

        parseInterfaces(*data.root, result.interfaces);
        parseActions(*data.root, result.interfaces);

        return result;
    }
};


} // unnamed namespace

GlobalConfig globalConfig;
DeviceConfig deviceConfig;

void initConfig(std::ostream& debug, DebugStreambuf& debugStream, EspApi& esp,
        Rtc& rtc, MqttClient& mqttClient) {
    ConfigParser(debug, debugStream, esp, rtc, mqttClient).parse();
}
