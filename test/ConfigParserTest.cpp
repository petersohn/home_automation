#include <gtest/gtest.h>

#include <optional>
#include <variant>

#include "DebugTestBase.hpp"
#include "common/ArduinoJson.hpp"
#include "common/ConfigParser.hpp"

using namespace ArduinoJson;

class ConfigParserTest : public DebugTestBase {
public:
    ConfigParser parser{this->debug};
    DynamicJsonBuffer buffer{512};

    JsonObject& parse(const std::string& json) {
        return buffer.parseObject(json);
    }
};

TEST_F(ConfigParserTest, ParseDebugEnabled_True) {
    auto& root = parse(R"({"debug": true})");
    EXPECT_TRUE(parser.parseDebugEnabled(root));
}

TEST_F(ConfigParserTest, ParseDebugEnabled_False) {
    auto& root = parse(R"({"debug": false})");
    EXPECT_FALSE(parser.parseDebugEnabled(root));
}

TEST_F(ConfigParserTest, ParseDebugEnabled_Missing) {
    auto& root = parse(R"({})");
    EXPECT_FALSE(parser.parseDebugEnabled(root));
}

TEST_F(ConfigParserTest, ParseGlobalConfig_NewStyleServers) {
    auto& root = parse(R"({
        "wifiSSID": "myssid", "wifiPassword": "mypass",
        "servers": [
            {"address": "192.168.1.1", "port": 1883, "username": "u1", "password": "p1"},
            {"address": "192.168.1.2", "port": 1884, "username": "u2", "password": "p2"}
        ]
    })");
    auto result = parser.parseGlobalConfig(root);
    EXPECT_EQ(result.wifiSSID, "myssid");
    EXPECT_EQ(result.wifiPassword, "mypass");
    ASSERT_EQ(result.servers.size(), 2u);
    EXPECT_EQ(result.servers[0].address, "192.168.1.1");
    EXPECT_EQ(result.servers[0].port, 1883u);
    EXPECT_EQ(result.servers[1].address, "192.168.1.2");
    EXPECT_EQ(result.servers[1].port, 1884u);
}

TEST_F(ConfigParserTest, ParseGlobalConfig_OldStyleSingleServer) {
    auto& root = parse(R"({
        "wifiSSID": "ssid",
        "serverAddress": "10.0.0.1", "serverPort": 1883,
        "serverUsername": "admin", "serverPassword": "secret"
    })");
    auto result = parser.parseGlobalConfig(root);
    EXPECT_EQ(result.wifiSSID, "ssid");
    ASSERT_EQ(result.servers.size(), 1u);
    EXPECT_EQ(result.servers[0].address, "10.0.0.1");
    EXPECT_EQ(result.servers[0].port, 1883u);
    EXPECT_EQ(result.servers[0].username, "admin");
    EXPECT_EQ(result.servers[0].password, "secret");
}

TEST_F(ConfigParserTest, ParseGlobalConfig_Empty) {
    auto& root = parse(R"({})");
    auto result = parser.parseGlobalConfig(root);
    EXPECT_EQ(result.wifiSSID, "");
    ASSERT_EQ(result.servers.size(), 1u);
    EXPECT_EQ(result.servers[0].address, "");
}

TEST_F(ConfigParserTest, ParseDeviceConfig_TopLevelFields) {
    auto& root = parse(R"({
        "name": "testDevice",
        "availabilityTopic": "dev/test/avail", "statusTopic": "dev/test/status",
        "debugTopic": "dev/test/debug", "debugPort": 1234, "resetPin": 5, "debug": true
    })");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.common.name, "testDevice");
    EXPECT_EQ(result.common.topics.availabilityTopic, "dev/test/avail");
    EXPECT_EQ(result.common.topics.statusTopic, "dev/test/status");
    EXPECT_EQ(result.common.debugTopic, "dev/test/debug");
    EXPECT_EQ(result.common.debugPort, 1234);
    EXPECT_EQ(result.common.resetPin, 5);
    EXPECT_TRUE(result.common.debug);
}

TEST_F(ConfigParserTest, ParseDeviceConfig_Defaults) {
    auto& root = parse(R"({})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.common.debugPort, 2534);
    EXPECT_EQ(result.common.resetPin, 255);
    EXPECT_FALSE(result.common.debug);
    EXPECT_TRUE(result.interfaces.empty());
}

TEST_F(ConfigParserTest, ParseInput_Valid) {
    auto& root = parse(
        R"({"interfaces":[{"type":"input","name":"btn","cycle":"single","pin":0,"debounce":50}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<InputConfig>(result.interfaces.at("btn").config);
    EXPECT_EQ(cfg.pin, 0);
    EXPECT_EQ(cfg.cycleType, CycleType::single);
    EXPECT_EQ(cfg.debounce, 50u);
}

TEST_F(ConfigParserTest, ParseInput_Defaults) {
    auto& root =
        parse(R"({"interfaces":[{"type":"input","name":"btn","pin":3}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<InputConfig>(result.interfaces.at("btn").config);
    EXPECT_EQ(cfg.cycleType, CycleType::single);
    EXPECT_EQ(cfg.debounce, 10u);
}

TEST_F(ConfigParserTest, ParseInput_MissingPin_Skipped) {
    auto& root = parse(R"({"interfaces":[{"type":"input","name":"btn"}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

TEST_F(ConfigParserTest, ParseOutput_Valid) {
    auto& root = parse(
        R"({"interfaces":[{"type":"output","name":"led","pin":2,"default":true,"invert":true}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<OutputConfig>(result.interfaces.at("led").config);
    EXPECT_EQ(cfg.pin, 2);
    EXPECT_TRUE(cfg.defaultValue);
    EXPECT_TRUE(cfg.invert);
}

TEST_F(ConfigParserTest, ParseOutput_Defaults) {
    auto& root =
        parse(R"({"interfaces":[{"type":"output","name":"led","pin":2}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<OutputConfig>(result.interfaces.at("led").config);
    EXPECT_FALSE(cfg.defaultValue);
    EXPECT_FALSE(cfg.invert);
}

TEST_F(ConfigParserTest, ParsePwm_Valid) {
    auto& root = parse(
        R"({"interfaces":[{"type":"pwm","name":"dim","pin":4,"default":128,"invert":true}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<PwmConfig>(result.interfaces.at("dim").config);
    EXPECT_EQ(cfg.pin, 4);
    EXPECT_EQ(cfg.defaultValue, 128);
    EXPECT_TRUE(cfg.invert);
}

TEST_F(ConfigParserTest, ParsePwm_Defaults) {
    auto& root =
        parse(R"({"interfaces":[{"type":"pwm","name":"dim","pin":4}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<PwmConfig>(result.interfaces.at("dim").config);
    EXPECT_EQ(cfg.defaultValue, 0);
    EXPECT_FALSE(cfg.invert);
}

TEST_F(ConfigParserTest, ParseAnalog_InternalInput) {
    auto& root = parse(
        R"({"interfaces":[{"type":"analog","name":"s","input":"","max":100.0,"precision":2}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<AnalogConfig>(result.interfaces.at("s").config);
    EXPECT_EQ(cfg.input.inputName, "");
    EXPECT_EQ(cfg.max, 100.0);
    EXPECT_EQ(cfg.precision, 2);
}

TEST_F(ConfigParserTest, ParseAnalog_NamedInput) {
    auto& root = parse(R"({
        "analogInputs":[{"name":"mcp","type":"mcp3008","sck":1,"mosi":2,"miso":3,"cs":4}],
        "interfaces":[{"type":"analog","name":"s","input":"mcp.5"}]
    })");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<AnalogConfig>(result.interfaces.at("s").config);
    EXPECT_EQ(cfg.input.inputName, "mcp");
    EXPECT_EQ(cfg.input.channel, 5);
}

TEST_F(ConfigParserTest, ParseAnalog_InvalidNamedInput_Skipped) {
    auto& root = parse(
        R"({"interfaces":[{"type":"analog","name":"s","input":"nonexistent.0"}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

TEST_F(ConfigParserTest, ParseEncoder_Valid) {
    auto& root = parse(
        R"({"interfaces":[{"type":"encoder","name":"enc","downPin":1,"upPin":2,"pulse":true}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<EncoderConfig>(result.interfaces.at("enc").config);
    EXPECT_EQ(cfg.downPin, 1);
    EXPECT_EQ(cfg.upPin, 2);
    EXPECT_TRUE(cfg.pulse);
}

TEST_F(ConfigParserTest, ParseEncoder_MissingPins_Skipped) {
    auto& root = parse(R"({"interfaces":[{"type":"encoder","name":"enc"}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

TEST_F(ConfigParserTest, ParseDht_Dht11) {
    auto& root = parse(
        R"({"interfaces":[{"type":"dht","name":"t","pin":5,"dhtType":"dht11"}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    EXPECT_EQ(cfg.pin, 5);
    EXPECT_EQ(cfg.type, dht_type::DHT11);
}

TEST_F(ConfigParserTest, ParseDht_DefaultDht22) {
    auto& root = parse(R"({"interfaces":[{"type":"dht","name":"t","pin":5}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    EXPECT_EQ(cfg.type, dht_type::DHT22);
}

TEST_F(ConfigParserTest, ParseDallasTemperature_Valid) {
    auto& root = parse(
        R"({"interfaces":[{"type":"dallasTemperature","name":"ds","pin":4,"devices":3}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg =
        std::get<DallasTemperatureConfig>(result.interfaces.at("ds").config);
    EXPECT_EQ(cfg.pin, 4);
    EXPECT_EQ(cfg.devices, 3u);
}

TEST_F(ConfigParserTest, ParseDallasTemperature_DefaultDevices) {
    auto& root = parse(
        R"({"interfaces":[{"type":"dallasTemperature","name":"ds","pin":4}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg =
        std::get<DallasTemperatureConfig>(result.interfaces.at("ds").config);
    EXPECT_EQ(cfg.devices, 1u);
}

TEST_F(ConfigParserTest, ParseHm3301_Valid) {
    auto& root = parse(
        R"({"interfaces":[{"type":"hm3301","name":"dust","sda":1,"scl":2}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<Hm3301Config>(result.interfaces.at("dust").config);
    EXPECT_EQ(cfg.sda, 1);
    EXPECT_EQ(cfg.scl, 2);
}

TEST_F(ConfigParserTest, ParseHm3301_MissingPins_Skipped) {
    auto& root = parse(R"({"interfaces":[{"type":"hm3301","name":"dust"}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

TEST_F(ConfigParserTest, ParseCounter_Valid) {
    auto& root = parse(
        R"({"interfaces":[{"type":"counter","name":"rain","pin":0,"multiplier":1800,"bounceTime":50}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<CounterConfig>(result.interfaces.at("rain").config);
    EXPECT_EQ(cfg.name, "rain");
    EXPECT_EQ(cfg.pin, 0);
    EXPECT_EQ(cfg.multiplier, 1800.0f);
    EXPECT_EQ(cfg.bounceTime, 50);
}

TEST_F(ConfigParserTest, ParseCounter_Defaults) {
    auto& root =
        parse(R"({"interfaces":[{"type":"counter","name":"cnt","pin":5}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<CounterConfig>(result.interfaces.at("cnt").config);
    EXPECT_EQ(cfg.multiplier, 1.0f);
    EXPECT_EQ(cfg.bounceTime, 0);
}

TEST_F(ConfigParserTest, ParseEchoDistance_WithTrigger) {
    auto& root = parse(
        R"({"interfaces":[{"type":"echo-distance","name":"dist","echoPin":1,"triggerPin":2,"triggerTime":20}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg =
        std::get<EchoDistanceConfig>(result.interfaces.at("dist").config);
    EXPECT_EQ(cfg.echoPin, 1);
    EXPECT_EQ(cfg.triggerPin, 2);
    EXPECT_EQ(cfg.triggerTime, 20u);
}

TEST_F(ConfigParserTest, ParseEchoDistance_WithoutTrigger_ReaderVariant) {
    auto& root = parse(
        R"({"interfaces":[{"type":"echo-distance","name":"dist","echoPin":3}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg =
        std::get<EchoDistanceReaderConfig>(result.interfaces.at("dist").config);
    EXPECT_EQ(cfg.echoPin, 3);
}

TEST_F(ConfigParserTest, ParseMqttInterface_Valid) {
    auto& root = parse(
        R"({"interfaces":[{"type":"mqtt","name":"mir","topic":"cmnd/topic"}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg =
        std::get<MqttInterfaceConfig>(result.interfaces.at("mir").config);
    EXPECT_EQ(cfg.topic, "cmnd/topic");
}

TEST_F(ConfigParserTest, ParseMqttInterface_EmptyTopic_Skipped) {
    auto& root =
        parse(R"({"interfaces":[{"type":"mqtt","name":"mir","topic":""}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

TEST_F(ConfigParserTest, ParseKeepalive_Valid) {
    auto& root = parse(
        R"({"interfaces":[{"type":"keepalive","name":"ka","pin":6,"interval":5000,"resetInterval":20}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<KeepaliveConfig>(result.interfaces.at("ka").config);
    EXPECT_EQ(cfg.pin, 6);
    EXPECT_EQ(cfg.interval, 5000u);
    EXPECT_EQ(cfg.resetInterval, 20u);
}

TEST_F(ConfigParserTest, ParseKeepalive_Defaults) {
    auto& root =
        parse(R"({"interfaces":[{"type":"keepalive","name":"ka","pin":6}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<KeepaliveConfig>(result.interfaces.at("ka").config);
    EXPECT_EQ(cfg.interval, 10000u);
    EXPECT_EQ(cfg.resetInterval, 10u);
}

TEST_F(ConfigParserTest, ParsePowerSupply_Valid) {
    auto& root = parse(
        R"({"interfaces":[{"type":"powerSupply","name":"ps","powerSwitchPin":1,"resetSwitchPin":2,"powerCheckPin":3,"pushTime":100,"forceOffTime":5000,"checkTime":30000,"initialState":"on"}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<PowerSupplyConfig>(result.interfaces.at("ps").config);
    EXPECT_EQ(cfg.powerSwitchPin, 1);
    EXPECT_EQ(cfg.resetSwitchPin, 2);
    EXPECT_EQ(cfg.powerCheckPin, 3);
    EXPECT_EQ(cfg.pushTime, 100u);
    EXPECT_EQ(cfg.forceOffTime, 5000u);
    EXPECT_EQ(cfg.checkTime, 30000u);
    EXPECT_EQ(cfg.initialState, "on");
}

TEST_F(ConfigParserTest, ParsePowerSupply_Defaults) {
    auto& root = parse(
        R"({"interfaces":[{"type":"powerSupply","name":"ps","powerSwitchPin":1,"resetSwitchPin":2,"powerCheckPin":3}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<PowerSupplyConfig>(result.interfaces.at("ps").config);
    EXPECT_EQ(cfg.pushTime, 200u);
    EXPECT_EQ(cfg.forceOffTime, 6000u);
    EXPECT_EQ(cfg.checkTime, 60000u);
    EXPECT_EQ(cfg.initialState, "");
}

TEST_F(ConfigParserTest, ParsePowerSupply_MissingPins_Skipped) {
    auto& root = parse(
        R"({"interfaces":[{"type":"powerSupply","name":"ps","powerSwitchPin":1}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

TEST_F(ConfigParserTest, ParseCover_Valid) {
    auto& root = parse(R"({
        "interfaces":[{"type":"cover","name":"gate",
            "upMovementPin":1,"downMovementPin":2,"upPin":3,"downPin":4,
            "stopPin":5,"latching":true,"invertInput":true,"invertOutput":true,
            "closedPosition":50,"invertPositionSensors":true,
            "positionSensors":[
                {"position":0,"pin":6,"invert":true},
                {"position":100,"pin":7,"invert":false}
            ]
        }]
    })");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<CoverConfig>(result.interfaces.at("gate").config);
    EXPECT_EQ(cfg.upMovementPin, 1);
    EXPECT_EQ(cfg.downMovementPin, 2);
    EXPECT_EQ(cfg.upPin, 3);
    EXPECT_EQ(cfg.downPin, 4);
    EXPECT_EQ(cfg.stopPin, 5);
    EXPECT_TRUE(cfg.latching);
    EXPECT_TRUE(cfg.invertInput);
    EXPECT_TRUE(cfg.invertOutput);
    EXPECT_EQ(cfg.closedPosition, 50);
    EXPECT_TRUE(cfg.invertPositionSensors);
    ASSERT_EQ(cfg.positionSensors.size(), 2u);
    EXPECT_EQ(cfg.positionSensors[0].position, 0);
    EXPECT_EQ(cfg.positionSensors[0].pin, 6);
    EXPECT_TRUE(cfg.positionSensors[0].invert);
    EXPECT_EQ(cfg.positionSensors[1].position, 100);
    EXPECT_EQ(cfg.positionSensors[1].pin, 7);
    EXPECT_FALSE(cfg.positionSensors[1].invert);
}

TEST_F(ConfigParserTest, ParseCover_PerSensorInvert) {
    auto& root = parse(R"({
        "interfaces":[{"type":"cover","name":"gate",
            "upMovementPin":1,"downMovementPin":2,"upPin":3,"downPin":4,
            "positionSensors":[{"position":0,"pin":6,"invert":true}]
        }]
    })");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<CoverConfig>(result.interfaces.at("gate").config);
    ASSERT_EQ(cfg.positionSensors.size(), 1u);
    EXPECT_TRUE(cfg.positionSensors[0].invert);
}

TEST_F(ConfigParserTest, ParseCover_Defaults) {
    auto& root = parse(
        R"({"interfaces":[{"type":"cover","name":"gate","upMovementPin":1,"downMovementPin":2,"upPin":3,"downPin":4}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<CoverConfig>(result.interfaces.at("gate").config);
    EXPECT_FALSE(cfg.latching);
    EXPECT_FALSE(cfg.invertInput);
    EXPECT_FALSE(cfg.invertOutput);
    EXPECT_EQ(cfg.closedPosition, 0);
    EXPECT_FALSE(cfg.invertPositionSensors);
    EXPECT_EQ(cfg.stopPin, 0);
    EXPECT_TRUE(cfg.positionSensors.empty());
}

TEST_F(ConfigParserTest, ParseCover_MissingPins_Skipped) {
    auto& root = parse(
        R"({"interfaces":[{"type":"cover","name":"gate","upMovementPin":1}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

TEST_F(ConfigParserTest, ParseStatus_Valid) {
    auto& root = parse(R"({"interfaces":[{"type":"status","name":"stat"}]})");
    auto result = parser.parseDeviceConfig(root);
    ASSERT_EQ(result.interfaces.size(), 1u);
    EXPECT_TRUE(
        std::holds_alternative<StatusConfig>(
            result.interfaces.at("stat").config));
}

TEST_F(ConfigParserTest, ParseInterface_InvalidType_Skipped) {
    auto& root = parse(R"({"interfaces":[{"type":"nonexistent","name":"x"}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

TEST_F(ConfigParserTest, ParseInterface_CommandTopic) {
    auto& root = parse(
        R"({"interfaces":[{"type":"output","name":"led","pin":2,"commandTopic":"dev/test/led/set"}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.at("led").commandTopic, "dev/test/led/set");
}

TEST_F(ConfigParserTest, SensorConfig_IntervalMs) {
    auto& root = parse(
        R"({"interfaces":[{"type":"dht","name":"t","pin":0,"intervalMs":5000}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    EXPECT_EQ(cfg.timing.interval, 5000);
}

TEST_F(ConfigParserTest, SensorConfig_IntervalSeconds) {
    auto& root = parse(
        R"({"interfaces":[{"type":"dht","name":"t","pin":0,"interval":10}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    EXPECT_EQ(cfg.timing.interval, 10000);
}

TEST_F(ConfigParserTest, SensorConfig_IntervalDefault) {
    auto& root = parse(R"({"interfaces":[{"type":"dht","name":"t","pin":0}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    EXPECT_EQ(cfg.timing.interval, 60000);
}

TEST_F(ConfigParserTest, SensorConfig_Offset) {
    auto& root = parse(
        R"({"interfaces":[{"type":"dht","name":"t","pin":0,"offset":5}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    EXPECT_EQ(cfg.timing.offset, 5000);
}

TEST_F(ConfigParserTest, SensorConfig_PulseString) {
    auto& root = parse(
        R"({"interfaces":[{"type":"dht","name":"t","pin":0,"pulse":"on"}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    ASSERT_EQ(cfg.timing.pulse.size(), 1u);
    EXPECT_EQ(cfg.timing.pulse[0], "on");
}

TEST_F(ConfigParserTest, SensorConfig_PulseArray) {
    auto& root = parse(
        R"({"interfaces":[{"type":"dht","name":"t","pin":0,"pulse":["a","b","c"]}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    ASSERT_EQ(cfg.timing.pulse.size(), 3u);
}

TEST_F(ConfigParserTest, SensorConfig_PulseMissing) {
    auto& root = parse(R"({"interfaces":[{"type":"dht","name":"t","pin":0}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    EXPECT_TRUE(cfg.timing.pulse.empty());
}

TEST_F(ConfigParserTest, AnalogInputWithChannel_Internal) {
    auto& root =
        parse(R"({"interfaces":[{"type":"analog","name":"s","input":""}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<AnalogConfig>(result.interfaces.at("s").config);
    EXPECT_EQ(cfg.input.inputName, "");
    EXPECT_EQ(cfg.input.channel, 0);
}

TEST_F(ConfigParserTest, AnalogInputWithChannel_Named) {
    auto& root = parse(R"({
        "analogInputs":[{"name":"mcp","type":"mcp3008","sck":1,"mosi":2,"miso":3,"cs":4}],
        "interfaces":[{"type":"analog","name":"s","input":"mcp.3"}]
    })");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<AnalogConfig>(result.interfaces.at("s").config);
    EXPECT_EQ(cfg.input.inputName, "mcp");
    EXPECT_EQ(cfg.input.channel, 3);
}

TEST_F(ConfigParserTest, ParseAnalogInputs_Mcp3008) {
    auto& root = parse(
        R"({"analogInputs":[{"name":"mcp","type":"mcp3008","sck":1,"mosi":2,"miso":3,"cs":4}]})");
    auto result = parser.parseDeviceConfig(root);
    ASSERT_EQ(result.analogInputs.size(), 1u);
    auto& mcp = std::get<Mcp3008Config>(result.analogInputs.at("mcp").input);
    EXPECT_EQ(mcp.sck, 1);
    EXPECT_EQ(mcp.mosi, 2);
    EXPECT_EQ(mcp.miso, 3);
    EXPECT_EQ(mcp.cs, 4);
}

TEST_F(ConfigParserTest, ParseAnalogInputs_MissingName_Skipped) {
    auto& root = parse(
        R"({"analogInputs":[{"type":"mcp3008","sck":1,"mosi":2,"miso":3,"cs":4}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.analogInputs.size(), 0u);
}

TEST_F(ConfigParserTest, ParseAnalogInputs_InvalidType_Skipped) {
    auto& root = parse(R"({"analogInputs":[{"name":"bad","type":"unknown"}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.analogInputs.size(), 0u);
}

TEST_F(ConfigParserTest, CrossRef_AnalogInputNotFound_Skipped) {
    auto& root = parse(
        R"({"interfaces":[{"type":"analog","name":"s","input":"nonexistent.0"}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

TEST_F(ConfigParserTest, CrossRef_ActionInterfaceNotFound_Skipped) {
    auto& root = parse(R"({
        "interfaces":[{"type":"input","name":"btn","pin":0}],
        "actions":[{"type":"publish","interface":"nonexistent","topic":"t"}]
    })");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.actions.size(), 0u);
}

TEST_F(ConfigParserTest, CrossRef_CommandTargetNotFound_Skipped) {
    auto& root = parse(R"({
        "interfaces":[{"type":"input","name":"btn","pin":0}],
        "actions":[{"type":"command","interface":"btn","target":"nonexistent","command":"toggle"}]
    })");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.actions.size(), 0u);
}

TEST_F(ConfigParserTest, ParseActions_PublishValid) {
    auto& root = parse(R"({
        "interfaces":[{"type":"input","name":"btn","pin":0}],
        "actions":[{"type":"publish","interface":"btn","topic":"t/state","retain":true,"minimumSendInterval":1000,"sendDiff":0.5,"payload":"%1"}]
    })");
    auto result = parser.parseDeviceConfig(root);
    ASSERT_EQ(result.actions.size(), 1u);
    auto& cfg = std::get<PublishActionConfig>(result.actions[0].config);
    EXPECT_EQ(cfg.topic, "t/state");
    EXPECT_TRUE(cfg.retain);
    EXPECT_EQ(cfg.minimumSendInterval, 1000u);
    EXPECT_EQ(cfg.sendDiff, 0.5);
    EXPECT_FALSE(cfg.operationJson.empty());
}

TEST_F(ConfigParserTest, ParseActions_PublishMissingTopic_Skipped) {
    auto& root = parse(R"({
        "interfaces":[{"type":"input","name":"btn","pin":0}],
        "actions":[{"type":"publish","interface":"btn"}]
    })");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.actions.size(), 0u);
}

TEST_F(ConfigParserTest, ParseActions_PublishDefaultTemplate) {
    auto& root = parse(R"({
        "interfaces":[{"type":"input","name":"btn","pin":0}],
        "actions":[{"type":"publish","interface":"btn","topic":"t"}]
    })");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<PublishActionConfig>(result.actions[0].config);
    EXPECT_NE(cfg.operationJson.find("%1"), std::string::npos);
}

TEST_F(ConfigParserTest, ParseActions_CommandValid) {
    auto& root = parse(R"({
        "interfaces":[
            {"type":"input","name":"btn","pin":0},
            {"type":"output","name":"led","pin":2}
        ],
        "actions":[{"type":"command","interface":"btn","target":"led","command":"toggle"}]
    })");
    auto result = parser.parseDeviceConfig(root);
    ASSERT_EQ(result.actions.size(), 1u);
    auto& cfg = std::get<CommandActionConfig>(result.actions[0].config);
    EXPECT_EQ(cfg.target, "led");
    EXPECT_FALSE(cfg.operationJson.empty());
}

TEST_F(ConfigParserTest, ParseActions_InvalidType_Skipped) {
    auto& root = parse(R"({
        "interfaces":[{"type":"input","name":"btn","pin":0}],
        "actions":[{"type":"invalid","interface":"btn"}]
    })");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.actions.size(), 0u);
}
