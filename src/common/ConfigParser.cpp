#include "ConfigParser.hpp"

#include <algorithm>
#include <memory>

#include "../tools/collection.hpp"
#include "ArduinoJson.hpp"

using namespace ArduinoJson;

namespace {

template <typename T>
std::optional<InterfaceConfigVariant> toVariant(std::optional<T> config) {
    return config ? std::optional<InterfaceConfigVariant>(*config)
                  : std::nullopt;
}

}  // unnamed namespace

ConfigParser::ConfigParser(std::ostream& debug) : debug(debug) {}

SensorConfig ConfigParser::getSensorConfig(const JsonObject& data) {
    SensorConfig result;
    auto intervalMs = data["intervalMs"].as<int>();
    if (intervalMs != 0) {
        result.interval = intervalMs;
    } else {
        result.interval = getJsonWithDefault(data["interval"], 60) * 1000;
    }
    result.offset = getJsonWithDefault(data["offset"], 0) * 1000;

    auto pulse = data.get<JsonVariant>("pulse");
    if (!pulse.success()) {
        return result;
    }

    auto& array = pulse.as<JsonArray>();
    if (array == JsonArray::invalid()) {
        result.pulse = {pulse.as<std::string>()};
    } else {
        result.pulse.reserve(array.size());
        for (const JsonVariant& value : array) {
            result.pulse.push_back(value.as<std::string>());
        }
    }

    return result;
}

CycleType ConfigParser::getCycleType(const std::string& value) {
    if (value == "none") {
        return CycleType::none;
    }
    if (value == "multi") {
        return CycleType::multi;
    }
    return CycleType::single;
}

const std::initializer_list<std::pair<const char*, int>> dhtTypes{
    {"", dht_type::DHT22},
    {"dht11", dht_type::DHT11},
    {"dht22", dht_type::DHT22},
    {"dht21", dht_type::DHT21}};

int ConfigParser::getDhtType(const std::string& value) {
    auto type = tools::findValue(dhtTypes, value);
    return type ? *type : dht_type::DHT22;
}

bool ConfigParser::parseDebugEnabled(const JsonObject& root) {
    JsonVariant rawValue = root["debug"];
    if (rawValue.is<bool>()) {
        return rawValue.as<bool>();
    }
    return false;
}

namespace {

ServerConfig parseServerConfig(const JsonObject& data) {
    ServerConfig result;
    result.address = data.get<std::string>("address");
    result.port = data.get<uint16_t>("port");
    result.username = data.get<std::string>("username");
    result.password = data.get<std::string>("password");
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

}  // namespace

GlobalConfig ConfigParser::parseGlobalConfig(const JsonObject& root) {
    GlobalConfig result;

    auto wifiSSID = root.get<const char*>("wifiSSID");
    if (wifiSSID) {
        result.wifiSSID = wifiSSID;
    }
    auto wifiPassword = root.get<const char*>("wifiPassword");
    if (wifiPassword) {
        result.wifiPassword = wifiPassword;
    }

    JsonArray& servers = root.get<JsonArray>("servers");
    if (servers == JsonArray::invalid()) {
        this->debug << "No servers config. "
                       "Attempting old-style single-server config."
                    << std::endl;
        result.servers.push_back(parseSingleServerConfig(root));
    } else {
        for (auto server : servers) {
            result.servers.push_back(
                parseServerConfig(server.as<JsonObject>()));
        }
    }

    return result;
}

std::optional<InputConfig> ConfigParser::parseInput(const JsonObject& data) {
    InputConfig result;
    if (!getPin(data, result.pin)) {
        return std::nullopt;
    }
    result.cycleType = getCycleType(data.get<std::string>("cycle"));
    result.debounce = getJsonWithDefault(data["debounce"], 10u);
    return result;
}

std::optional<OutputConfig> ConfigParser::parseOutput(const JsonObject& data) {
    OutputConfig result;
    if (!getPin(data, result.pin)) {
        return std::nullopt;
    }
    result.defaultValue = getJsonWithDefault(data["default"], false);
    result.invert = getJsonWithDefault(data["invert"], false);
    return result;
}

std::optional<PwmConfig> ConfigParser::parsePwm(const JsonObject& data) {
    PwmConfig result;
    if (!getPin(data, result.pin)) {
        return std::nullopt;
    }
    result.defaultValue = getJsonWithDefault(data["default"], 0);
    result.invert = getJsonWithDefault(data["invert"], false);
    return result;
}

std::optional<AnalogInputWithChannelDescription>
ConfigParser::parseAnalogInputWithChannel(const std::string& value) {
    if (value.empty()) {
        return AnalogInputWithChannelDescription{};
    }

    auto dotLocation = value.find('.');
    if (dotLocation == std::string::npos) {
        this->debug << "Invalid input format: " << value << std::endl;
        return std::nullopt;
    }

    auto name = value.substr(0, dotLocation);
    auto channelStr = value.substr(dotLocation + 1);
    StaticJsonBuffer<10> buf;
    auto v = buf.parse(channelStr);
    if (!v.is<uint8_t>()) {
        this->debug << "Not a valid channel number: " << channelStr
                    << std::endl;
        return std::nullopt;
    }

    AnalogInputWithChannelDescription result;
    result.inputName = name;
    result.channel = v.as<uint8_t>();
    return result;
}

std::optional<AnalogConfig> ConfigParser::parseAnalog(const JsonObject& data) {
    auto inputDesc =
        parseAnalogInputWithChannel(data["input"].as<std::string>());
    if (!inputDesc) {
        this->debug << "Invalid analog input." << std::endl;
        return std::nullopt;
    }
    AnalogConfig result;
    result.input = *inputDesc;
    result.max = getJsonWithDefault(data["max"], 0.0);
    result.valueOffset = getJsonWithDefault(data["valueOffset"], 0.0);
    result.cutoff = getJsonWithDefault(data["cutoff"], 0.0);
    result.precision = getJsonWithDefault(data["precision"], 0);
    result.aggregateTime = getJsonWithDefault(data["aggregateTime"], 0U);
    result.aggregateDelay = getJsonWithDefault(data["aggregateDelay"], 0U);
    result.timing = getSensorConfig(data);
    return result;
}

std::optional<EncoderConfig> ConfigParser::parseEncoder(
    const JsonObject& data) {
    EncoderConfig result;
    if (!getRequiredValue(data, "downPin", result.downPin) ||
        !getRequiredValue(data, "upPin", result.upPin)) {
        return std::nullopt;
    }
    result.pulse = getJsonWithDefault(data["pulse"], false);
    return result;
}

std::optional<DhtConfig> ConfigParser::parseDht(const JsonObject& data) {
    DhtConfig result;
    result.type = getDhtType(data.get<std::string>("dhtType"));
    if (!getPin(data, result.pin)) {
        return std::nullopt;
    }
    result.timing = getSensorConfig(data);
    return result;
}

std::optional<DallasTemperatureConfig> ConfigParser::parseDallasTemperature(
    const JsonObject& data) {
    DallasTemperatureConfig result;
    result.devices = data.get<size_t>("devices");
    if (result.devices == 0) {
        result.devices = 1;
    }
    if (!getPin(data, result.pin)) {
        return std::nullopt;
    }
    result.timing = getSensorConfig(data);
    return result;
}

std::optional<Hm3301Config> ConfigParser::parseHm3301(const JsonObject& data) {
    Hm3301Config result;
    if (!getRequiredValue(data, "sda", result.sda) ||
        !getRequiredValue(data, "scl", result.scl)) {
        return std::nullopt;
    }
    result.timing = getSensorConfig(data);
    return result;
}

std::optional<CounterConfig> ConfigParser::parseCounter(
    const JsonObject& data) {
    CounterConfig result;
    result.name = data.get<std::string>("name");
    if (!getPin(data, result.pin)) {
        return std::nullopt;
    }
    result.bounceTime = getJsonWithDefault(data["bounceTime"], 0);
    result.multiplier = getJsonWithDefault(data["multiplier"], 1.0f);
    result.timing = getSensorConfig(data);
    return result;
}

std::optional<InterfaceConfigVariant> ConfigParser::parseEchoDistance(
    const JsonObject& data) {
    EchoDistanceReaderConfig readerConfig;
    if (!getRequiredValue(data, "echoPin", readerConfig.echoPin)) {
        return std::nullopt;
    }

    uint8_t triggerPin =
        getJsonWithDefault(data["triggerPin"], static_cast<uint8_t>(0));
    if (triggerPin != 0) {
        EchoDistanceConfig result;
        result.echoPin = readerConfig.echoPin;
        result.triggerPin = triggerPin;
        result.triggerTime = getJsonWithDefault(data["triggerTime"], 10u);
        result.timing = getSensorConfig(data);
        return result;
    }
    return readerConfig;
}

std::optional<MqttInterfaceConfig> ConfigParser::parseMqttInterface(
    const JsonObject& data) {
    MqttInterfaceConfig result;
    result.topic = data["topic"].as<std::string>();
    if (result.topic.empty()) {
        return std::nullopt;
    }
    return result;
}

std::optional<KeepaliveConfig> ConfigParser::parseKeepalive(
    const JsonObject& data) {
    KeepaliveConfig result;
    if (!getPin(data, result.pin)) {
        return std::nullopt;
    }
    result.interval = getJsonWithDefault(data["interval"], 10000u);
    result.resetInterval = getJsonWithDefault(data["resetInterval"], 10u);
    return result;
}

std::optional<PowerSupplyConfig> ConfigParser::parsePowerSupply(
    const JsonObject& data) {
    PowerSupplyConfig result;
    if (!getRequiredValue(data, "powerSwitchPin", result.powerSwitchPin) ||
        !getRequiredValue(data, "resetSwitchPin", result.resetSwitchPin) ||
        !getRequiredValue(data, "powerCheckPin", result.powerCheckPin)) {
        return std::nullopt;
    }
    result.pushTime = getJsonWithDefault(data["pushTime"], 200u);
    result.forceOffTime = getJsonWithDefault(data["forceOffTime"], 6000u);
    result.checkTime = getJsonWithDefault(data["checkTime"], 60000u);
    result.initialState =
        getJsonWithDefault(data["initialState"], std::string(""));
    return result;
}

std::optional<CoverConfig> ConfigParser::parseCover(const JsonObject& data) {
    CoverConfig result;
    const JsonArray& positionSensorConfigs = data["positionSensors"];
    result.positionSensors.reserve(positionSensorConfigs.size());
    for (const JsonObject& sensorConfig : positionSensorConfigs) {
        PositionSensor sensor;
        if (!getRequiredValue(sensorConfig, "position", sensor.position) ||
            !getRequiredValue(sensorConfig, "pin", sensor.pin)) {
            continue;
        }
        sensor.invert = getJsonWithDefault(sensorConfig["invert"], false);
        result.positionSensors.push_back(sensor);
    }

    if (!getRequiredValue(data, "upMovementPin", result.upMovementPin) ||
        !getRequiredValue(data, "downMovementPin", result.downMovementPin) ||
        !getRequiredValue(data, "upPin", result.upPin) ||
        !getRequiredValue(data, "downPin", result.downPin)) {
        return std::nullopt;
    }
    result.stopPin =
        getJsonWithDefault(data["stopPin"], static_cast<uint8_t>(0));
    result.latching = getJsonWithDefault(data["latching"], false);
    result.invertInput = getJsonWithDefault(data["invertInput"], false);
    result.invertOutput = getJsonWithDefault(data["invertOutput"], false);
    result.closedPosition = getJsonWithDefault(data["closedPosition"], 0);
    result.invertPositionSensors =
        getJsonWithDefault(data["invertPositionSensors"], false);
    return result;
}

std::optional<StatusConfig> ConfigParser::parseStatus(const JsonObject&) {
    return StatusConfig{};
}

std::optional<InterfaceConfigVariant> ConfigParser::parseInterface(
    const JsonObject& data) {
    std::string type = data.get<std::string>("type");
    if (type == "input") {
        return toVariant(parseInput(data));
    } else if (type == "output") {
        return toVariant(parseOutput(data));
    } else if (type == "pwm") {
        return toVariant(parsePwm(data));
    } else if (type == "analog") {
        return toVariant(parseAnalog(data));
    } else if (type == "encoder") {
        return toVariant(parseEncoder(data));
    } else if (type == "dht") {
        return toVariant(parseDht(data));
    } else if (type == "dallasTemperature") {
        return toVariant(parseDallasTemperature(data));
    } else if (type == "hm3301") {
        return toVariant(parseHm3301(data));
    } else if (type == "counter") {
        return toVariant(parseCounter(data));
    } else if (type == "hc-sr04" || type == "echo-distance") {
        return parseEchoDistance(data);
    } else if (type == "mqtt") {
        return toVariant(parseMqttInterface(data));
    } else if (type == "keepalive") {
        return toVariant(parseKeepalive(data));
    } else if (type == "powerSupply") {
        return toVariant(parsePowerSupply(data));
    } else if (type == "cover") {
        return toVariant(parseCover(data));
    } else if (type == "status") {
        return toVariant(parseStatus(data));
    }
    this->debug << "Invalid interface type: " << type << std::endl;
    return std::nullopt;
}

std::optional<AnalogInputConfig> ConfigParser::parseAnalogInput(
    const JsonObject& data) {
    std::string type = data["type"];
    if (type.empty()) {
        this->debug << "Input needs a valid type.\n";
        return std::nullopt;
    }

    if (type == "mcp3008") {
        Mcp3008Config mcp;
        if (!getRequiredValue(data, "sck", mcp.sck) ||
            !getRequiredValue(data, "mosi", mcp.mosi) ||
            !getRequiredValue(data, "miso", mcp.miso) ||
            !getRequiredValue(data, "cs", mcp.cs)) {
            return std::nullopt;
        }
        AnalogInputConfig result;
        result.name = data["name"].as<std::string>();
        result.input = mcp;
        return result;
    }
    this->debug << "Invalid input type: " << type << "\n";
    return std::nullopt;
}

std::unordered_map<std::string, InterfaceEntry> ConfigParser::parseInterfaces(
    const JsonObject& data,
    const std::unordered_map<std::string, AnalogInputConfig>& analogInputs) {
    std::unordered_map<std::string, InterfaceEntry> result;

    const JsonArray& interfaces = data["interfaces"];
    if (interfaces == JsonArray::invalid()) {
        this->debug << "Could not parse interfaces." << std::endl;
        return result;
    }

    for (const JsonObject& interface : interfaces) {
        if (interface == JsonObject::invalid()) {
            this->debug << "Interface configuration must be an object."
                        << std::endl;
            continue;
        }

        auto parsed = parseInterface(interface);
        if (!parsed) {
            this->debug << "Invalid interface configuration." << std::endl;
            continue;
        }

        if (std::holds_alternative<AnalogConfig>(*parsed)) {
            auto& analogCfg = std::get<AnalogConfig>(*parsed);
            if (!analogCfg.input.inputName.empty() &&
                analogInputs.find(analogCfg.input.inputName) ==
                    analogInputs.end()) {
                this->debug
                    << "Analog input not found: " << analogCfg.input.inputName
                    << std::endl;
                continue;
            }
        }

        InterfaceEntry entry;
        entry.name = interface.get<std::string>("name");
        entry.commandTopic = interface.get<std::string>("commandTopic");
        entry.config = std::move(*parsed);
        result[entry.name] = std::move(entry);
    }

    return result;
}

std::string ConfigParser::serializeOperationJson(const JsonObject& data) {
    bool hasPayload = data["payload"].success();
    bool hasCommand = data["command"].success();
    bool hasTemplate = data["template"].success();

    if (!hasPayload && !hasCommand && !hasTemplate) {
        return R"({"template":"%1"})";
    }

    std::string result;
    StaticJsonBuffer<512> buf;
    JsonObject& obj = buf.createObject();
    if (hasPayload) {
        obj["payload"] = data["payload"];
    }
    if (hasCommand) {
        obj["command"] = data["command"];
    }
    if (hasTemplate) {
        obj["template"] = data["template"];
    }
    obj.printTo(result);
    return result;
}

std::optional<ActionConfigVariant> ConfigParser::parseAction(
    const JsonObject& data) {
    auto type = data.get<std::string>("type");

    if (type == "publish") {
        PublishActionConfig result;
        result.topic = data.get<std::string>("topic");
        if (result.topic.empty()) {
            this->debug << "topic is mandatory." << std::endl;
            return std::nullopt;
        }
        result.operationJson = serializeOperationJson(data);
        result.retain = data.get<bool>("retain");
        result.minimumSendInterval = data.get<unsigned>("minimumSendInterval");
        result.sendDiff = data.get<double>("sendDiff");
        return result;
    } else if (type == "command") {
        CommandActionConfig result;
        result.target = data.get<std::string>("target");
        if (result.target.empty()) {
            this->debug << "target is mandatory." << std::endl;
            return std::nullopt;
        }
        result.operationJson = serializeOperationJson(data);
        return result;
    }

    this->debug << "Invalid action type: " << type << std::endl;
    return std::nullopt;
}

std::vector<ActionEntry> ConfigParser::parseActions(
    const JsonObject& data,
    const std::unordered_map<std::string, InterfaceEntry>& interfaces) {
    std::vector<ActionEntry> result;

    const JsonArray& actions = data["actions"];
    if (actions == JsonArray::invalid()) {
        this->debug << "Could not parse actions." << std::endl;
        return result;
    }

    for (const JsonObject& action : actions) {
        const std::string interfaceName = action["interface"];
        if (interfaces.find(interfaceName) == interfaces.end()) {
            this->debug << "Interface not found: " << interfaceName
                        << std::endl;
            continue;
        }

        auto type = action.get<std::string>("type");
        if (type == "command") {
            const std::string targetName = action["target"];
            if (interfaces.find(targetName) == interfaces.end()) {
                this->debug << "Interface not found: " << targetName
                            << std::endl;
                continue;
            }
        }

        auto parsed = parseAction(action);
        if (!parsed) {
            this->debug << "Invalid action configuration." << std::endl;
            continue;
        }

        ActionEntry entry;
        entry.interface = interfaceName;
        entry.config = std::move(*parsed);
        result.push_back(std::move(entry));
    }

    return result;
}

DeviceConfigDescription ConfigParser::parseDeviceConfig(
    const JsonObject& root) {
    DeviceConfigDescription result;

    auto name = root.get<const char*>("name");
    if (name) {
        result.common.name = name;
    }
    auto availTopic = root.get<const char*>("availabilityTopic");
    if (availTopic) {
        result.common.topics.availabilityTopic = availTopic;
    }
    auto statusTopic = root.get<const char*>("statusTopic");
    if (statusTopic) {
        result.common.topics.statusTopic = statusTopic;
    }
    auto debugTopic = root.get<const char*>("debugTopic");
    if (debugTopic) {
        result.common.debugTopic = debugTopic;
    }
    result.common.debug = parseDebugEnabled(root);
    result.common.debugPort = getJsonWithDefault(root["debugPort"], 2534);
    result.common.resetPin =
        getJsonWithDefault(root["resetPin"], static_cast<uint8_t>(255));

    {
        const JsonArray& inputs = root["analogInputs"];
        if (inputs != JsonArray::invalid()) {
            for (const JsonObject& input : inputs) {
                if (input == JsonObject::invalid()) {
                    this->debug << "Input configuration must be an object."
                                << std::endl;
                    continue;
                }
                std::string name = input["name"];
                if (name.empty()) {
                    this->debug << "Input needs a valid name." << std::endl;
                    continue;
                }
                auto parsed = parseAnalogInput(input);
                if (!parsed) {
                    this->debug << name << ": invalid config." << std::endl;
                    continue;
                }
                result.analogInputs[std::move(name)] = std::move(*parsed);
            }
        }
    }

    result.interfaces = parseInterfaces(root, result.analogInputs);
    result.actions = parseActions(root, result.interfaces);

    return result;
}