#include "config.hpp"

#include <FS.h>

#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <variant>

#include "CounterInterface.hpp"
#include "DallasTemperatureSensor.hpp"
#include "DebugStream.hpp"
#include "DhtSensor.hpp"
#include "EchoDistanceReaderInterface.hpp"
#include "EchoDistanceSensor.hpp"
#include "EncoderInterface.hpp"
#include "EspAnalogInput.hpp"
#include "EspEncoder.hpp"
#include "GpioInput.hpp"
#include "GpioOutput.hpp"
#include "HM3301Sensor.hpp"
#include "JsonParser.hpp"
#include "KeepaliveInterface.hpp"
#include "Mcp3008AnalogInput.hpp"
#include "MqttInterface.hpp"
#include "PowerSupplyInterface.hpp"
#include "PublishAction.hpp"
#include "PwmOutput.hpp"
#include "StatusInterface.hpp"
#include "common/AnalogInput.hpp"
#include "common/AnalogInputWithChannel.hpp"
#include "common/AnalogSensor.hpp"
#include "common/ArduinoJson.hpp"
#include "common/CommandAction.hpp"
#include "common/ConfigParser.hpp"
#include "common/Cover.hpp"
#include "common/MqttClient.hpp"
#include "common/SensorInterface.hpp"
#include "operation/OperationParser.hpp"
#include "operation/OperationParser2.hpp"
#include "tools/collection.hpp"

using namespace ArduinoJson;

namespace {

class ConfigFactory {
public:
    ConfigFactory(
        std::ostream& debug, DebugStreambuf& debugStream, EspApi& esp, Rtc& rtc,
        MqttClient& mqttClient);

    void initSerial();
    DeviceConfig buildDeviceConfig(DeviceConfigDescription&& parsed);

private:
    std::ostream& debug;
    DebugStreambuf& debugStream;
    EspApi& esp;
    Rtc& rtc;
    MqttClient& mqttClient;
    std::unique_ptr<std::streambuf> debugStreambuf;
    std::unordered_map<std::string, std::shared_ptr<AnalogInput>> analogInputs;

    std::unique_ptr<Interface> buildInterface(
        const InterfaceConfigVariant& config);

    std::unique_ptr<Interface> buildInput(const InputConfig& c);
    std::unique_ptr<Interface> buildOutput(const OutputConfig& c);
    std::unique_ptr<Interface> buildPwm(const PwmConfig& c);
    std::unique_ptr<Interface> buildAnalog(const AnalogConfig& c);
    std::unique_ptr<Interface> buildEncoder(const EncoderConfig& c);
    std::unique_ptr<Interface> buildDht(const DhtConfig& c);
    std::unique_ptr<Interface> buildDallasTemperature(
        const DallasTemperatureConfig& c);
    std::unique_ptr<Interface> buildHm3301(const Hm3301Config& c);
    std::unique_ptr<Interface> buildCounter(const CounterConfig& c);
    std::unique_ptr<Interface> buildEchoDistance(const EchoDistanceConfig& c);
    std::unique_ptr<Interface> buildEchoDistanceReader(
        const EchoDistanceReaderConfig& c);
    std::unique_ptr<Interface> buildMqttInterface(const MqttInterfaceConfig& c);
    std::unique_ptr<Interface> buildKeepalive(const KeepaliveConfig& c);
    std::unique_ptr<Interface> buildPowerSupply(const PowerSupplyConfig& c);
    std::unique_ptr<Interface> buildCover(const CoverConfig& c);
    std::unique_ptr<Interface> buildStatus(const StatusConfig& c);

    AnalogInputWithChannel getEspAnalogInput();
    std::optional<AnalogInputWithChannel> resolveAnalogInput(
        const AnalogInputWithChannelDescription& desc);

    std::unique_ptr<Interface> createSensorInterface(
        const SensorConfig& timing, std::unique_ptr<Sensor>&& sensor,
        const std::string& name);

    std::pair<
        std::unique_ptr<operation::Operation>,
        std::unordered_set<InterfaceConfig*>>
    parseOperation(
        const std::vector<std::unique_ptr<InterfaceConfig>>& interfaces,
        InterfaceConfig* defaultInterface, const ArduinoJson::JsonObject& data,
        const char* fieldName, const char* templateFieldName);

    void buildActions(
        const std::vector<ActionEntry>& entries,
        std::vector<std::unique_ptr<InterfaceConfig>>& interfaces);
};

ConfigFactory::ConfigFactory(
    std::ostream& debug, DebugStreambuf& debugStream, EspApi& esp, Rtc& rtc,
    MqttClient& mqttClient)
    : debug(debug)
    , debugStream(debugStream)
    , esp(esp)
    , rtc(rtc)
    , mqttClient(mqttClient) {}

void ConfigFactory::initSerial() {
    Serial.begin(115200);
    this->debugStreambuf = std::make_unique<PrintStreambuf>(Serial);
    this->debugStream.add(this->debugStreambuf.get());
}

std::unique_ptr<Interface> ConfigFactory::buildInput(const InputConfig& c) {
    return std::make_unique<GpioInput>(this->debug, c);
}

std::unique_ptr<Interface> ConfigFactory::buildOutput(const OutputConfig& c) {
    return std::make_unique<GpioOutput>(this->debug, this->esp, this->rtc, c);
}

std::unique_ptr<Interface> ConfigFactory::buildPwm(const PwmConfig& c) {
    return std::make_unique<PwmOutput>(this->debug, this->esp, this->rtc, c);
}

std::unique_ptr<Interface> ConfigFactory::buildAnalog(const AnalogConfig& c) {
    auto input = this->resolveAnalogInput(c.input);
    if (!input) {
        this->debug << "Invalid analog input." << std::endl;
        return nullptr;
    }
    return this->createSensorInterface(
        c.timing,
        std::make_unique<AnalogSensor>(
            this->esp, this->debug, std::move(*input), c),
        "");
}

std::unique_ptr<Interface> ConfigFactory::buildEncoder(const EncoderConfig& c) {
    return std::make_unique<EncoderInterface>(
        std::make_unique<EspEncoder>(c.downPin, c.upPin), c);
}

std::unique_ptr<Interface> ConfigFactory::buildDht(const DhtConfig& c) {
    return this->createSensorInterface(
        c.timing, std::make_unique<DhtSensor>(this->debug, c), "");
}

std::unique_ptr<Interface> ConfigFactory::buildDallasTemperature(
    const DallasTemperatureConfig& c) {
    return this->createSensorInterface(
        c.timing, std::make_unique<DallasTemperatureSensor>(this->debug, c),
        "");
}

std::unique_ptr<Interface> ConfigFactory::buildHm3301(const Hm3301Config& c) {
    return this->createSensorInterface(
        c.timing, std::make_unique<HM3301Sensor>(this->debug, c), "");
}

std::unique_ptr<Interface> ConfigFactory::buildCounter(const CounterConfig& c) {
    return std::make_unique<CounterInterface>(this->debug, this->esp, c);
}

std::unique_ptr<Interface> ConfigFactory::buildEchoDistance(
    const EchoDistanceConfig& c) {
    return this->createSensorInterface(
        c.timing,
        std::make_unique<EchoDistanceSensor>(this->debug, this->esp, c), "");
}

std::unique_ptr<Interface> ConfigFactory::buildEchoDistanceReader(
    const EchoDistanceReaderConfig& c) {
    return std::make_unique<EchoDistanceReaderInterface>(
        this->debug, this->esp, c);
}

std::unique_ptr<Interface> ConfigFactory::buildMqttInterface(
    const MqttInterfaceConfig& c) {
    return std::make_unique<MqttInterface>(this->mqttClient, c);
}

std::unique_ptr<Interface> ConfigFactory::buildKeepalive(
    const KeepaliveConfig& c) {
    return std::make_unique<KeepaliveInterface>(this->esp, c);
}

std::unique_ptr<Interface> ConfigFactory::buildPowerSupply(
    const PowerSupplyConfig& c) {
    return std::make_unique<PowerSupplyInterface>(this->debug, this->esp, c);
}

std::unique_ptr<Interface> ConfigFactory::buildCover(const CoverConfig& c) {
    return std::make_unique<Cover>(this->debug, this->esp, this->rtc, c);
}

std::unique_ptr<Interface> ConfigFactory::buildStatus(const StatusConfig&) {
    return std::make_unique<StatusInterface>(this->mqttClient);
}

std::unique_ptr<Interface> ConfigFactory::createSensorInterface(
    const SensorConfig& timing, std::unique_ptr<Sensor>&& sensor,
    const std::string& name) {
    return std::make_unique<SensorInterface>(
        this->debug, this->esp, std::move(sensor), name, timing);
}

AnalogInputWithChannel ConfigFactory::getEspAnalogInput() {
    auto it = this->analogInputs.find("");
    if (it == this->analogInputs.end()) {
        it = this->analogInputs.emplace("", std::make_shared<EspAnalogInput>())
                 .first;
    }
    return AnalogInputWithChannel(it->second, A0);
}

std::optional<AnalogInputWithChannel> ConfigFactory::resolveAnalogInput(
    const AnalogInputWithChannelDescription& desc) {
    if (desc.inputName.empty()) {
        return this->getEspAnalogInput();
    }

    auto it = this->analogInputs.find(desc.inputName);
    if (it == this->analogInputs.end()) {
        this->debug << "Input not found: " << desc.inputName << std::endl;
        return std::nullopt;
    }
    return AnalogInputWithChannel(it->second, desc.channel);
}

std::unique_ptr<Interface> ConfigFactory::buildInterface(
    const InterfaceConfigVariant& config) {
    return std::visit([this](const auto& c) -> std::unique_ptr<Interface> {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, InputConfig>) {
            return this->buildInput(c);
        } else if constexpr (std::is_same_v<T, OutputConfig>) {
            return this->buildOutput(c);
        } else if constexpr (std::is_same_v<T, PwmConfig>) {
            return this->buildPwm(c);
        } else if constexpr (std::is_same_v<T, AnalogConfig>) {
            return this->buildAnalog(c);
        } else if constexpr (std::is_same_v<T, EncoderConfig>) {
            return this->buildEncoder(c);
        } else if constexpr (std::is_same_v<T, DhtConfig>) {
            return this->buildDht(c);
        } else if constexpr (std::is_same_v<T, DallasTemperatureConfig>) {
            return this->buildDallasTemperature(c);
        } else if constexpr (std::is_same_v<T, Hm3301Config>) {
            return this->buildHm3301(c);
        } else if constexpr (std::is_same_v<T, CounterConfig>) {
            return this->buildCounter(c);
        } else if constexpr (std::is_same_v<T, EchoDistanceConfig>) {
            return this->buildEchoDistance(c);
        } else if constexpr (std::is_same_v<T, EchoDistanceReaderConfig>) {
            return this->buildEchoDistanceReader(c);
        } else if constexpr (std::is_same_v<T, MqttInterfaceConfig>) {
            return this->buildMqttInterface(c);
        } else if constexpr (std::is_same_v<T, KeepaliveConfig>) {
            return this->buildKeepalive(c);
        } else if constexpr (std::is_same_v<T, PowerSupplyConfig>) {
            return this->buildPowerSupply(c);
        } else if constexpr (std::is_same_v<T, CoverConfig>) {
            return this->buildCover(c);
        } else if constexpr (std::is_same_v<T, StatusConfig>) {
            return this->buildStatus(c);
        } else {
            static_assert(
                !std::is_same_v<T, T>, "unhandled InterfaceConfigVariant type");
            return nullptr;
        }
    }, config);
}

std::pair<
    std::unique_ptr<operation::Operation>, std::unordered_set<InterfaceConfig*>>
ConfigFactory::parseOperation(
    const std::vector<std::unique_ptr<InterfaceConfig>>& interfaces,
    InterfaceConfig* defaultInterface, const ArduinoJson::JsonObject& data,
    const char* fieldName, const char* templateFieldName) {
    if (data[fieldName].is<std::string>()) {
        operation::Parser2 parser{this->debug, interfaces, defaultInterface};
        auto operation = parser.parse(data.get<std::string>(fieldName));
        auto usedInterfaces = std::move(parser).getUsedInterfaces();
        return {std::move(operation), std::move(usedInterfaces)};
    }
    operation::Parser parser{interfaces, defaultInterface};
    auto operation = parser.parse(data, fieldName, templateFieldName);
    auto usedInterfaces = std::move(parser).getUsedInterfaces();
    return {std::move(operation), std::move(usedInterfaces)};
}

void ConfigFactory::buildActions(
    const std::vector<ActionEntry>& entries,
    std::vector<std::unique_ptr<InterfaceConfig>>& interfaces) {
    for (const auto& entry : entries) {
        auto defaultInterface = findInterface(interfaces, entry.interface);
        if (!defaultInterface) {
            this->debug << "Interface not found: "
                        << entry.interface << std::endl;
            continue;
        }

        std::visit([this, defaultInterface, &interfaces](const auto& cfg) {
            using T = std::decay_t<decltype(cfg)>;
            if constexpr (std::is_same_v<T, PublishActionConfig>) {
                DynamicJsonBuffer buffer{512};
                auto& root = buffer.parseObject(cfg.operationJson);
                auto [operation, usedInterfaces] = this->parseOperation(
                    interfaces, defaultInterface, root, "payload", "template");

                auto action = std::make_shared<PublishAction>(
                    this->debug, this->esp, this->mqttClient, cfg.topic,
                    std::move(operation), cfg.retain, cfg.minimumSendInterval,
                    cfg.sendDiff);

                defaultInterface->hasExternalAction = true;
                for (auto* iface : usedInterfaces) {
                    iface->hasExternalAction = true;
                }
                usedInterfaces.insert(defaultInterface);
                for (auto* iface : usedInterfaces) {
                    iface->actions.push_back(action);
                }
            } else if constexpr (std::is_same_v<T, CommandActionConfig>) {
                auto target = findInterface(interfaces, cfg.target);
                if (!target) {
                    this->debug << "Interface not found: " << cfg.target
                                << std::endl;
                    return;
                }

                DynamicJsonBuffer buffer{512};
                auto& root = buffer.parseObject(cfg.operationJson);
                auto [operation, usedInterfaces] = this->parseOperation(
                    interfaces, defaultInterface, root, "command", "template");

                auto action = std::make_shared<CommandAction>(
                    *target->interface, std::move(operation));

                defaultInterface->hasInternalAction = true;
                for (auto* iface : usedInterfaces) {
                    iface->hasInternalAction = true;
                }
                usedInterfaces.insert(defaultInterface);
                for (auto* iface : usedInterfaces) {
                    iface->actions.push_back(action);
                }
            } else {
                static_assert(
                    !std::is_same_v<T, T>,
                    "unhandled ActionConfigVariant type");
            }
        }, entry.config);
    }
}

DeviceConfig ConfigFactory::buildDeviceConfig(
    DeviceConfigDescription&& parsed) {
    DeviceConfig result;
    result.common = parsed.common;
    result.debug = std::move(this->debugStreambuf);

    if (result.common.resetPin <= 16) {
        this->esp.pinMode(result.common.resetPin, GpioMode::input);
    }

    this->debug << "\nStarting up...\n"
                << "Debug port = " << result.common.debugPort
                << ", reset pin = " << static_cast<int>(result.common.resetPin)
                << std::endl;

    for (auto& [name, inputConfig] : parsed.analogInputs) {
        auto analogInput =
            std::visit([](const auto& cfg) -> std::shared_ptr<AnalogInput> {
            using T = std::decay_t<decltype(cfg)>;
            if constexpr (std::is_same_v<T, Mcp3008Config>) {
                return std::make_shared<Mcp3008AnalogInput>(
                    cfg.sck, cfg.mosi, cfg.miso, cfg.cs);
            } else {
                static_assert(
                    !std::is_same_v<T, T>,
                    "unhandled AnalogInputConfig variant type");
                return nullptr;
            }
        }, inputConfig.input);
        if (analogInput) {
            this->analogInputs[std::move(name)] = std::move(analogInput);
        }
    }

    for (auto& [name, entry] : parsed.interfaces) {
        auto interface = this->buildInterface(entry.config);
        if (!interface) {
            this->debug << "Invalid interface configuration." << std::endl;
            continue;
        }

        auto interfaceConfig = std::make_unique<InterfaceConfig>();
        interfaceConfig->name = name;
        interfaceConfig->interface = std::move(interface);

        if (!entry.commandTopic.empty()) {
            this->mqttClient.subscribe(
                entry.commandTopic.c_str(),
                [&interfaceConfig](const MqttConnection::Message& message) {
                std::string command{message.payload, message.payloadLength};
                interfaceConfig->interface->execute(command);
            });
        }

        result.interfaces.push_back(std::move(interfaceConfig));
    }

    this->buildActions(parsed.actions, result.interfaces);

    return result;
}

}  // unnamed namespace

GlobalConfig globalConfig;
DeviceConfig deviceConfig;

void initConfig(
    std::ostream& debug, DebugStreambuf& debugStream, EspApi& esp, Rtc& rtc,
    MqttClient& mqttClient) {
    SPIFFS.begin();
    JsonParser jsonParser(debug);

    ParsedData globalData = jsonParser.parseFile("/global_config.json");
    ParsedData deviceData = jsonParser.parseFile("/device_config.json");

    ConfigParser parser(debug);
    ConfigFactory factory(debug, debugStream, esp, rtc, mqttClient);

    if (deviceData.root && parser.parseDebugEnabled(*deviceData.root)) {
        factory.initSerial();
    }

    globalConfig = globalData.root ? parser.parseGlobalConfig(*globalData.root)
                                   : GlobalConfig{};

    DeviceConfigDescription parsedDevice =
        deviceData.root ? parser.parseDeviceConfig(*deviceData.root)
                        : DeviceConfigDescription{};

    deviceConfig = factory.buildDeviceConfig(std::move(parsedDevice));
}
