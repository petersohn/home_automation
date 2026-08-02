#ifndef COMMON_CONFIG_PARSER_HPP
#define COMMON_CONFIG_PARSER_HPP

#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "ActionConfig.hpp"
#include "AnalogInputConfig.hpp"
#include "GlobalConfig.hpp"
#include "InterfaceConfigs.hpp"
#include "common/ArduinoJson.hpp"

struct DeviceConfigDescription {
    DeviceConfigCommon common;
    std::unordered_map<std::string, AnalogInputConfig> analogInputs;
    std::unordered_map<std::string, InterfaceEntry> interfaces;
    std::vector<ActionEntry> actions;
};

class ConfigParser {
public:
    explicit ConfigParser(std::ostream& debug);

    bool parseDebugEnabled(const ArduinoJson::JsonObject& root);
    GlobalConfig parseGlobalConfig(const ArduinoJson::JsonObject& root);
    DeviceConfigDescription parseDeviceConfig(
        const ArduinoJson::JsonObject& root);

private:
    std::ostream& debug;

    template <typename T>
    bool getRequiredValue(
        const ArduinoJson::JsonObject& data, const char* name, T& value) {
        auto rawValue = data[name];
        if (rawValue.is<T>()) {
            value = rawValue.as<T>();
            return true;
        }
        this->debug << "Invalid " << name << ": " << rawValue.as<std::string>()
                    << std::endl;
        return false;
    }

    bool getPin(const ArduinoJson::JsonObject& data, uint8_t& value) {
        return getRequiredValue(data, "pin", value);
    }

    template <typename T>
    T getJsonWithDefault(ArduinoJson::JsonVariant data, T defaultValue) {
        return data | defaultValue;
    }

    SensorConfig getSensorConfig(const ArduinoJson::JsonObject& data);
    CycleType getCycleType(const std::string& value);
    int getDhtType(const std::string& value);

    std::optional<InterfaceConfigVariant> parseInterface(
        const ArduinoJson::JsonObject& data);
    std::optional<InputConfig> parseInput(const ArduinoJson::JsonObject& data);
    std::optional<OutputConfig> parseOutput(
        const ArduinoJson::JsonObject& data);
    std::optional<PwmConfig> parsePwm(const ArduinoJson::JsonObject& data);
    std::optional<AnalogConfig> parseAnalog(
        const ArduinoJson::JsonObject& data);
    std::optional<EncoderConfig> parseEncoder(
        const ArduinoJson::JsonObject& data);
    std::optional<DhtConfig> parseDht(const ArduinoJson::JsonObject& data);
    std::optional<DallasTemperatureConfig> parseDallasTemperature(
        const ArduinoJson::JsonObject& data);
    std::optional<Hm3301Config> parseHm3301(
        const ArduinoJson::JsonObject& data);
    std::optional<CounterConfig> parseCounter(
        const ArduinoJson::JsonObject& data);
    std::optional<InterfaceConfigVariant> parseEchoDistance(
        const ArduinoJson::JsonObject& data);
    std::optional<MqttInterfaceConfig> parseMqttInterface(
        const ArduinoJson::JsonObject& data);
    std::optional<KeepaliveConfig> parseKeepalive(
        const ArduinoJson::JsonObject& data);
    std::optional<PowerSupplyConfig> parsePowerSupply(
        const ArduinoJson::JsonObject& data);
    std::optional<CoverConfig> parseCover(const ArduinoJson::JsonObject& data);
    std::optional<StatusConfig> parseStatus(
        const ArduinoJson::JsonObject& data);

    std::optional<AnalogInputConfig> parseAnalogInput(
        const ArduinoJson::JsonObject& data);
    std::optional<AnalogInputWithChannelDescription>
    parseAnalogInputWithChannel(const std::string& value);

    std::unordered_map<std::string, InterfaceEntry> parseInterfaces(
        const ArduinoJson::JsonObject& data,
        const std::unordered_map<std::string, AnalogInputConfig>& analogInputs);
    std::vector<ActionEntry> parseActions(
        const ArduinoJson::JsonObject& data,
        const std::unordered_map<std::string, InterfaceEntry>& interfaces);
    std::optional<ActionConfigVariant> parseAction(
        const ArduinoJson::JsonObject& data);

    std::string serializeOperationJson(const ArduinoJson::JsonObject& data);
};

#endif  // COMMON_CONFIG_PARSER_HPP