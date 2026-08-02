# Config Parser Testability Design

## Goal

Make `src/config.cpp` unit testable by separating pure JSON parsing from
device-specific object creation. Input: config JSON. Output: description structs
that tests can inspect.

## Background

`ConfigParser` (config.cpp:47-674) couples pure JSON parsing with:

- Concrete `Interface` object creation (GpioInput, DhtSensor, Cover, etc.)
- Hardware calls (SPIFFS file reads, Serial init, `esp.pinMode`)
- `mqttClient.subscribe` for command topics
- Operation parsing (needs concrete `InterfaceConfig*` pointers)

Result: none of the config parsing logic is unit tested. The only way to verify
config behavior is manual flash testing.

## Approach

Split into three layers with strict dependency direction:

```
src/common/ConfigDescription headers  -- pure data structs (no device deps)
src/common/ConfigParser.hpp/.cpp      -- JSON -> config structs (no device deps, testable)
src/config.cpp                        -- config structs -> concrete objects (+ SPIFFS/Serial/mqtt)
```

### Description Struct Approach

`std::variant` of per-type config structs. Compiler-enforced exhaustive dispatch
in factory via `std::visit`. Adding a new interface type is a compile error until
factory handles it. No heap allocation per description. Matches existing
value-semantic style (PositionSensor, ServerConfig are flat value structs).

Rejected alternatives:

- Polymorphic description hierarchy: open hierarchy, but `dynamic_cast` dispatch,
  heap per description, easy to miss a type in factory.
- Flat struct + type tag enum: simplest, but no compiler exhaustiveness check,
  unused fields, easy to forget setting a field.

## File Layout

### Config struct headers (src/common/)

One header per interface config, grouped only when same interface uses both:

```
SensorConfig.hpp              -- SensorConfig (shared by sensor interfaces)
InputConfig.hpp               -- InputConfig + CycleType enum
OutputConfig.hpp              -- OutputConfig
PwmConfig.hpp                 -- PwmConfig
AnalogConfig.hpp              -- AnalogConfig + AnalogInputWithChannelDescription
AnalogInputConfig.hpp         -- AnalogInputConfig + Mcp3008Config
EncoderConfig.hpp             -- EncoderConfig
DhtConfig.hpp                 -- DhtConfig + DHT constants
DallasTemperatureConfig.hpp  -- DallasTemperatureConfig
Hm3301Config.hpp              -- Hm3301Config
CounterConfig.hpp             -- CounterConfig
EchoDistanceConfig.hpp        -- EchoDistanceConfig + EchoDistanceReaderConfig
MqttInterfaceConfig.hpp       -- MqttInterfaceConfig
KeepaliveConfig.hpp           -- KeepaliveConfig
PowerSupplyConfig.hpp         -- PowerSupplyConfig
CoverConfig.hpp               -- CoverConfig + PositionSensor
StatusConfig.hpp              -- StatusConfig
GlobalConfig.hpp              -- ServerConfig, GlobalConfig, TopicConfig, DeviceConfigCommon
ActionConfig.hpp              -- PublishActionConfig, CommandActionConfig, ActionConfigVariant, ActionEntry
InterfaceConfigs.hpp          -- InterfaceConfigVariant + InterfaceEntry (aggregation)
ConfigParser.hpp              -- DeviceConfigDescription + ConfigParser class
ConfigParser.cpp              -- pure parsing implementation
```

### Rules

- Each per-interface config header: pure data, no device deps, no includes except
  std + shared helpers.
- `SensorConfig.hpp` standalone -- used by AnalogConfig, DhtConfig,
  DallasTemperatureConfig, Hm3301Config, CounterConfig, EchoDistanceConfig.
- `CoverConfig.hpp` keeps PositionSensor + CoverConfig together (both only used
  by Cover + parser).
- `EchoDistanceConfig.hpp` keeps both echo configs together (same subsystem).
- `AnalogInputConfig.hpp` keeps Mcp3008Config + AnalogInputConfig (analog input
  subsystem).
- `AnalogConfig.hpp` defines `AnalogInputWithChannelDescription` (input name +
  channel, references AnalogInputConfig by name).
- `InterfaceConfigs.hpp` is the aggregation point -- includes all per-type
  headers, defines the variant + InterfaceEntry. Device code and factory include
  this.
- `GlobalConfig.hpp`: ServerConfig moves here from config.hpp (no device deps).
  DeviceConfigCommon is the shared part between DeviceConfigDescription and
  device DeviceConfig.
- `CycleType` enum lives in InputConfig.hpp; GpioInput.hpp includes it.
- DHT constants (DHT11, DHT21, DHT22) live in DhtConfig.hpp; DhtSensor.hpp includes
  them.

## Config Structs

### SensorConfig (shared by sensor interfaces)

```cpp
struct SensorConfig {
    int interval = 60000;
    int offset = 0;
    std::vector<std::string> pulse;
};
```

### Per-interface configs

```cpp
struct InputConfig {
    uint8_t pin = 0;
    CycleType cycleType = CycleType::single;
    unsigned debounce = 10;
};

struct OutputConfig {
    uint8_t pin = 0;
    bool defaultValue = false;
    bool invert = false;
};

struct PwmConfig {
    uint8_t pin = 0;
    int defaultValue = 0;
    bool invert = false;
};

struct AnalogInputWithChannelDescription {
    std::string inputName;  // "" = internal ESP analog
    uint8_t channel = 0;    // ignored for internal
};

struct AnalogConfig {
    AnalogInputWithChannelDescription input;
    double max = 0, valueOffset = 0, cutoff = 0;
    int precision = 0;
    unsigned aggregateTime = 0, aggregateDelay = 0;
    SensorConfig timing;
};

struct Mcp3008Config {
    uint8_t sck = 0, mosi = 0, miso = 0, cs = 0;
};

struct AnalogInputConfig {
    std::string name;
    std::variant<Mcp3008Config> input;  // currently only mcp3008
};

struct EncoderConfig {
    uint8_t downPin = 0, upPin = 0;
    bool pulse = false;
};

struct DhtConfig {
    uint8_t pin = 0;
    int type = DHT22;
    SensorConfig timing;
};

struct DallasTemperatureConfig {
    uint8_t pin = 0;
    size_t devices = 1;
    SensorConfig timing;
};

struct Hm3301Config {
    int sda = 0, scl = 0;
    SensorConfig timing;
};

struct CounterConfig {
    std::string name;
    uint8_t pin = 0;
    int bounceTime = 0;
    float multiplier = 1.0f;
    SensorConfig timing;
};

struct EchoDistanceConfig {
    uint8_t echoPin = 0, triggerPin = 0;
    unsigned triggerTime = 10;
    SensorConfig timing;
};

struct EchoDistanceReaderConfig {
    uint8_t echoPin = 0;
};

struct MqttInterfaceConfig {
    std::string topic;
};

struct KeepaliveConfig {
    uint8_t pin = 0;
    unsigned interval = 10000, resetInterval = 10;
};

struct PowerSupplyConfig {
    uint8_t powerSwitchPin = 0, resetSwitchPin = 0, powerCheckPin = 0;
    unsigned pushTime = 200, forceOffTime = 6000, checkTime = 60000;
    std::string initialState;
};

struct PositionSensor {
    int position = 0;
    uint8_t pin = 0;
    bool invert = false;
};

struct CoverConfig {
    uint8_t upMovementPin = 0, downMovementPin = 0;
    uint8_t upPin = 0, downPin = 0, stopPin = 0;
    bool latching = false, invertInput = false, invertOutput = false;
    int closedPosition = 0;
    bool invertPositionSensors = false;
    std::vector<PositionSensor> positionSensors;
};

struct StatusConfig {};  // no fields
```

### Variant + entries

```cpp
using InterfaceConfigVariant = std::variant<
    InputConfig, OutputConfig, PwmConfig, AnalogConfig, EncoderConfig,
    DhtConfig, DallasTemperatureConfig, Hm3301Config, CounterConfig,
    EchoDistanceConfig, EchoDistanceReaderConfig, MqttInterfaceConfig,
    KeepaliveConfig, PowerSupplyConfig, CoverConfig, StatusConfig>;

struct InterfaceEntry {
    std::string name;
    std::string commandTopic;
    InterfaceConfigVariant config;
};
```

### Global / Device common

```cpp
struct ServerConfig {
    std::string address;
    uint16_t port = 0;
    std::string username;
    std::string password;
};

struct GlobalConfig {
    std::string wifiSSID;
    std::string wifiPassword;
    std::vector<ServerConfig> servers;
};

struct TopicConfig {
    std::string availabilityTopic;
    std::string statusTopic;
};

struct DeviceConfigCommon {
    std::string name;
    TopicConfig topics;
    int debugPort = 2534;
    std::string debugTopic;
    uint8_t resetPin = 255;
    bool debug = false;
};
```

### Actions

Operations stored as raw JSON string. Factory re-parses via operation::Parser /
Parser2 after concrete interfaces exist. Avoids duplicating operation parser in
common layer.

```cpp
struct PublishActionConfig {
    std::string topic;
    std::string operationJson;   // serialized {payload|template} sub-object
    bool retain = false;
    unsigned minimumSendInterval = 0;
    double sendDiff = 0.0;
};

struct CommandActionConfig {
    std::string target;
    std::string operationJson;   // serialized {command|template} sub-object
};

using ActionConfigVariant = std::variant<PublishActionConfig, CommandActionConfig>;

struct ActionEntry {
    std::string interface;  // default interface name
    ActionConfigVariant config;
};
```

### Parser output

```cpp
struct DeviceConfigDescription {
    DeviceConfigCommon common;
    std::unordered_map<std::string, AnalogInputConfig> analogInputs;
    std::unordered_map<std::string, InterfaceEntry> interfaces;
    std::vector<ActionEntry> actions;
};
```

`analogInputs` and `interfaces` are unordered_maps indexed by name so parser can
verify references (analog input names in AnalogConfig, interface names in
actions, command action targets).

## Parser API

### common/ConfigParser.hpp

```cpp
class ConfigParser {
public:
    explicit ConfigParser(std::ostream& debug);

    bool parseDebugEnabled(const ArduinoJson::JsonObject& root);

    GlobalConfig parseGlobalConfig(const ArduinoJson::JsonObject& root);
    DeviceConfigDescription parseDeviceConfig(const ArduinoJson::JsonObject& root);

private:
    std::ostream& debug;

    // shared helpers (moved from config.cpp ConfigParser, pure, no device deps)
    template <typename T>
    bool getRequiredValue(const JsonObject& data, const char* name, T& value);
    bool getPin(const JsonObject& data, uint8_t& value);
    template <typename T>
    T getJsonWithDefault(JsonVariant data, T defaultValue);
    SensorConfig getSensorConfig(const JsonObject& data);
    CycleType getCycleType(const std::string& value);
    int getDhtType(const std::string& value);

    // per-type parsers
    std::optional<InterfaceConfigVariant> parseInterface(const JsonObject& data);
    std::optional<InputConfig> parseInput(const JsonObject& data);
    std::optional<OutputConfig> parseOutput(const JsonObject& data);
    std::optional<PwmConfig> parsePwm(const JsonObject& data);
    std::optional<AnalogConfig> parseAnalog(const JsonObject& data);
    std::optional<EncoderConfig> parseEncoder(const JsonObject& data);
    std::optional<DhtConfig> parseDht(const JsonObject& data);
    std::optional<DallasTemperatureConfig> parseDallasTemperature(const JsonObject& data);
    std::optional<Hm3301Config> parseHm3301(const JsonObject& data);
    std::optional<CounterConfig> parseCounter(const JsonObject& data);
    std::optional<InterfaceConfigVariant> parseEchoDistance(const JsonObject& data);
    std::optional<MqttInterfaceConfig> parseMqttInterface(const JsonObject& data);
    std::optional<KeepaliveConfig> parseKeepalive(const JsonObject& data);
    std::optional<PowerSupplyConfig> parsePowerSupply(const JsonObject& data);
    std::optional<CoverConfig> parseCover(const JsonObject& data);
    std::optional<StatusConfig> parseStatus(const JsonObject& data);

    std::optional<AnalogInputConfig> parseAnalogInput(const JsonObject& data);
    std::optional<AnalogInputWithChannelDescription> parseAnalogInputWithChannel(const std::string& value);

    std::unordered_map<std::string, InterfaceEntry> parseInterfaces(
        const JsonObject& data,
        const std::unordered_map<std::string, AnalogInputConfig>& analogInputs);
    std::vector<ActionEntry> parseActions(
        const JsonObject& data,
        const std::unordered_map<std::string, InterfaceEntry>& interfaces);
    std::optional<ActionConfigVariant> parseAction(const JsonObject& data);
};
```

### Behavior

1. Input is JsonObject, not file. Parser takes already-parsed JSON root. File
   reading (SPIFFS) stays in config.cpp. Tests feed JSON via
   DynamicJsonBuffer::parseObject(jsonString).

2. Debug logging preserved. Parser takes std::ostream& debug. Tests use
   TestStreambuf (already exists). Invalid entries logged + skipped, same as
   current behavior.

3. getSensorConfig consolidates getInterval + getOffset + getPulse into one
   SensorConfig return.

4. parseInterface returns optional<variant>. nullopt = invalid config (logged +
   skipped by caller). Same skip-on-invalid pattern as current.

5. parseDeviceConfig parsing order: analogInputs first, then interfaces (can
   verify analog input refs), then actions (can verify interface refs).

6. AnalogInputWithChannelDescription parsing: parser splits "name.channel" into
   {inputName, channel}. Empty string = {inputName="", channel=0} (internal,
   valid). Non-empty must have dot + valid numeric channel, else invalid +
   logged. Parser validates format only; factory resolves to concrete
   AnalogInputWithChannel.

7. Cross-reference validation: parser verifies existence of named analog inputs
   in AnalogConfig, named interfaces in ActionEntry, named targets in
   CommandActionConfig. Non-existent refs logged + skipped.

8. Action operation stored as raw JSON string. parseAction serializes the
   entire action object (excluding type/interface/target/topic/retain/
   minimumSendInterval/sendDiff) to string via JsonObject::printTo(std::string).
   This preserves the operation field (payload or command) + template field
   together. Factory re-parses the whole string via operation::Parser/Parser2.
   If neither operation field nor template exists, default template to "%1"
   (current behavior: data.set("template", "%1")).

9. mqttClient.subscribe stays in factory. Parser only records commandTopic in
   InterfaceEntry.

10. Debug/Serial handling: parseDebugEnabled reads "debug" bool from root.
    Factory calls initSerial if true (Serial.begin + PrintStreambuf ->
    debugStream). Done BEFORE full parse so parser logs visible on Serial.
    Network debug (WifiStreambuf/MqttStreambuf) unaffected -- set up in
    main.cpp::setup() based on DeviceConfigCommon.debugPort/debugTopic, already
    after config parse.

## Factory & Device Config Wiring

### config.hpp (updated)

```cpp
#include "common/GlobalConfig.hpp"

struct DeviceConfig {
    DeviceConfigCommon common;
    std::unique_ptr<std::streambuf> debug;  // device-specific (Serial-backed)
    std::vector<std::unique_ptr<InterfaceConfig>> interfaces;  // concrete
};

extern GlobalConfig globalConfig;
extern DeviceConfig deviceConfig;

void initConfig(
    std::ostream& debug, DebugStreambuf& debugStream, EspApi& esp, Rtc& rtc,
    MqttClient& mqttClient);
```

### config.cpp structure

```cpp
namespace {

class ConfigFactory {
public:
    ConfigFactory(
        std::ostream& debug, DebugStreambuf& debugStream, EspApi& esp,
        Rtc& rtc, MqttClient& mqttClient);

    void initSerial();  // Serial.begin + PrintStreambuf -> debugStream
    DeviceConfig buildDeviceConfig(DeviceConfigDescription&& parsed);

private:
    std::ostream& debug;
    DebugStreambuf& debugStream;
    EspApi& esp;
    Rtc& rtc;
    MqttClient& mqttClient;
    std::unique_ptr<std::streambuf> debugStreambuf;  // from initSerial
    std::unordered_map<std::string, std::shared_ptr<AnalogInput>> analogInputs;

    // std::visit dispatch
    std::unique_ptr<Interface> buildInterface(const InterfaceConfigVariant& config);

    // per-type builders
    std::unique_ptr<Interface> buildInput(const InputConfig& c);
    std::unique_ptr<Interface> buildOutput(const OutputConfig& c);
    std::unique_ptr<Interface> buildPwm(const PwmConfig& c);
    std::unique_ptr<Interface> buildAnalog(const AnalogConfig& c);
    std::unique_ptr<Interface> buildEncoder(const EncoderConfig& c);
    std::unique_ptr<Interface> buildDht(const DhtConfig& c);
    std::unique_ptr<Interface> buildDallasTemperature(const DallasTemperatureConfig& c);
    std::unique_ptr<Interface> buildHm3301(const Hm3301Config& c);
    std::unique_ptr<Interface> buildCounter(const CounterConfig& c);
    std::unique_ptr<Interface> buildEchoDistance(const EchoDistanceConfig& c);
    std::unique_ptr<Interface> buildEchoDistanceReader(const EchoDistanceReaderConfig& c);
    std::unique_ptr<Interface> buildMqttInterface(const MqttInterfaceConfig& c);
    std::unique_ptr<Interface> buildKeepalive(const KeepaliveConfig& c);
    std::unique_ptr<Interface> buildPowerSupply(const PowerSupplyConfig& c);
    std::unique_ptr<Interface> buildCover(const CoverConfig& c);
    std::unique_ptr<Interface> buildStatus(const StatusConfig& c);

    // analog input creation (device-specific)
    std::shared_ptr<AnalogInput> buildAnalogInput(const AnalogInputConfig& c);
    AnalogInputWithChannel getEspAnalogInput();
    std::optional<AnalogInputWithChannel> resolveAnalogInput(
        const AnalogInputWithChannelDescription& desc);

    // sensor interface wrapper
    std::unique_ptr<Interface> createSensorInterface(
        const SensorConfig& timing, std::unique_ptr<Sensor>&& sensor,
        const std::string& name);

    // actions
    std::shared_ptr<Action> buildAction(
        const ActionEntry& entry,
        std::vector<std::unique_ptr<InterfaceConfig>>& interfaces);
    std::shared_ptr<Action> buildPublishAction(
        const PublishActionConfig& c, InterfaceConfig* defaultInterface,
        std::vector<std::unique_ptr<InterfaceConfig>>& interfaces,
        bool InterfaceConfig::* actionType);
    std::shared_ptr<Action> buildCommandAction(
        const CommandActionConfig& c, InterfaceConfig* defaultInterface,
        std::vector<std::unique_ptr<InterfaceConfig>>& interfaces,
        bool InterfaceConfig::* actionType);

    // operation re-parsing
    std::pair<std::unique_ptr<operation::Operation>, std::unordered_set<InterfaceConfig*>>
    parseOperation(
        const std::string& operationJson,
        const std::vector<std::unique_ptr<InterfaceConfig>>& interfaces,
        InterfaceConfig* defaultInterface);

    void subscribeCommandTopic(
        const std::string& commandTopic, InterfaceConfig& interfaceConfig);
};

}  // unnamed namespace
```

### initConfig flow

```cpp
void initConfig(...) {
    SPIFFS.begin();
    JsonParser jsonParser(debug);

    ParsedData globalData = jsonParser.parseFile("/global_config.json");
    ParsedData deviceData = jsonParser.parseFile("/device_config.json");

    ConfigParser parser(debug);
    ConfigFactory factory(debug, debugStream, esp, rtc, mqttClient);

    if (deviceData.root && parser.parseDebugEnabled(*deviceData.root)) {
        factory.initSerial();
    }

    globalConfig = globalData.root
        ? parser.parseGlobalConfig(*globalData.root)
        : GlobalConfig{};

    DeviceConfigDescription parsedDevice = deviceData.root
        ? parser.parseDeviceConfig(*deviceData.root)
        : DeviceConfigDescription{};

    deviceConfig = factory.buildDeviceConfig(std::move(parsedDevice));
}
```

### buildDeviceConfig behavior

1. Moves PrintStreambuf from initSerial into deviceConfig.debug.
2. Calls esp.pinMode(common.resetPin, input) if resetPin <= 16.
3. Logs startup message (debug port, reset pin).
4. Builds analog inputs (creates EspAnalogInput/Mcp3008AnalogInput).
5. Builds interfaces (std::visit over variant -> concrete Interface). Iterates
   interfaces unordered_map, builds concrete objects, stores in
   DeviceConfig.interfaces vector. Skips nullptrs (logged + skipped, same as
   current).
6. For each InterfaceEntry: subscribes commandTopic via mqttClient.subscribe
   (same lambda as current).
7. Builds actions (re-parses operation JSON via operation::Parser/Parser2 with
   concrete InterfaceConfig pointers). Skips failures (logged).

### std::visit dispatch

```cpp
std::unique_ptr<Interface> ConfigFactory::buildInterface(
    const InterfaceConfigVariant& config) {
    return std::visit([this](const auto& c) -> std::unique_ptr<Interface> {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, InputConfig>)
            return buildInput(c);
        else if constexpr (std::is_same_v<T, OutputConfig>)
            return buildOutput(c);
        // ... one branch per type
        else
            return nullptr;
    }, config);
}
```

## Device Constructor Changes

Device constructors take config structs directly:

| Header | New constructor signature |
|--------|--------------------------|
| GpioInput.hpp | GpioInput(std::ostream& debug, const InputConfig& config) |
| GpioOutput.hpp | GpioOutput(std::ostream& debug, EspApi&, Rtc&, const OutputConfig&) |
| PwmOutput.hpp | PwmOutput(std::ostream& debug, EspApi&, Rtc&, const PwmConfig&) |
| AnalogSensor.hpp | AnalogSensor(EspApi&, std::ostream&, AnalogInputWithChannel, const AnalogConfig&) |
| EncoderInterface.hpp | EncoderInterface(std::unique_ptr<Encoder>, const EncoderConfig&) |
| DhtSensor.hpp | DhtSensor(std::ostream&, const DhtConfig&) |
| DallasTemperatureSensor.hpp | DallasTemperatureSensor(std::ostream&, const DallasTemperatureConfig&) |
| HM3301Sensor.hpp | HM3301Sensor(std::ostream&, const Hm3301Config&) |
| CounterInterface.hpp | CounterInterface(std::ostream&, EspApi&, const CounterConfig&) |
| EchoDistanceSensor.hpp | EchoDistanceSensor(std::ostream&, EspApi&, const EchoDistanceConfig&) |
| EchoDistanceReaderInterface.hpp | EchoDistanceReaderInterface(std::ostream&, EspApi&, const EchoDistanceReaderConfig&) |
| MqttInterface.hpp | MqttInterface(MqttClient&, const MqttInterfaceConfig&) |
| KeepaliveInterface.hpp | KeepaliveInterface(EspApi&, const KeepaliveConfig&) |
| PowerSupplyInterface.hpp | PowerSupplyInterface(std::ostream&, EspApi&, const PowerSupplyConfig&) |
| Cover.hpp | Cover(std::ostream&, EspApi&, Rtc&, const CoverConfig&) |
| StatusInterface.hpp | StatusInterface(MqttClient&) -- unchanged (no fields) |
| SensorInterface.hpp | SensorInterface(std::ostream&, EspApi&, std::unique_ptr<Sensor>, const std::string& name, const SensorConfig&) |

## Error Handling

### Parser (testable)

1. Missing required fields -> nullopt + log. parseInterface returns
   optional<InterfaceConfigVariant>. Missing pin, invalid type, etc. -> log
   message + nullopt. parseInterfaces skips nullopt entries. Same as current.

2. Invalid JSON root -> empty result. parseGlobalConfig/parseDeviceConfig with
   null root (file missing) -> returns default-constructed struct.

3. Old-style single-server config. parseGlobalConfig: if no servers array,
   parse single serverAddress/serverPort/serverUsername/serverPassword from
   root. Preserves backward compat.

4. Default values. Every config struct has defaults matching current
   getJsonWithDefault calls.

5. getPulse polymorphism. pulse field can be string or array. getSensorConfig
   handles both.

6. getInterval logic. intervalMs takes priority; else interval * 1000 with
   default 60.

7. AnalogInputWithChannelDescription parsing: parser validates name.channel
   format. Empty string = valid (internal, factory handles). Non-empty must
   have dot + valid numeric channel, else invalid + logged.

8. Actions -- operation stored as JSON string. parseAction serializes the
   entire action object (excluding type/interface/target/topic/retain/
   minimumSendInterval/sendDiff) to string. This preserves the operation field
   (payload or command) + template field together. Factory re-parses the whole
   string via operation::Parser/Parser2. If neither payload/command nor template
   exists, default template to "%1" (current behavior: data.set("template",
   "%1")).

9. Cross-reference validation: parser verifies existence of named analog
   inputs in AnalogConfig, named interfaces in ActionEntry, named targets in
   CommandActionConfig. Non-existent refs logged + skipped.

### Factory (device-specific, not unit tested)

1. Interface build failure. buildInterface returns nullptr -> buildDeviceConfig
   logs + skips.

2. Analog input resolution. Factory resolves AnalogInputWithChannelDescription
   -> AnalogInputWithChannel. Empty -> EspAnalogInput. Named -> lookup in
   analogInputs map + channel. Missing name -> log + skip interface.

3. mqttClient.subscribe. Factory subscribes commandTopic after building
   interface. Lambda captures InterfaceConfig& (same as current).

4. Action build failure. Missing target interface -> log + skip. Operation
   re-parse failure -> log + skip.

### Principle

Parser validates structure + fields, logs + skips invalid entries. Factory
validates device-specific references (interface names, analog input names),
logs + skips. Both non-throwing.

## Test Base

### DebugTestBase (new)

```cpp
// test/DebugTestBase.hpp
class DebugTestBase : public ::testing::Test {
public:
    TestStreambuf debugStreambuf;
    std::ostream debug;
    DebugTestBase();
};
```

```cpp
// test/DebugTestBase.cpp
#include "DebugTestBase.hpp"
DebugTestBase::DebugTestBase() : debug(&debugStreambuf) {}
```

### EspTestBase (updated)

```cpp
// test/EspTestBase.hpp
#include "DebugTestBase.hpp"
class EspTestBase : public DebugTestBase {
public:
    FakeEspApi esp;
    FakeRtc rtc;
    FakeWifi wifi;
    EspTestBase();
    ~EspTestBase();
    // delayUntil, expectLog unchanged
};
```

ConfigParserTest uses only debug stream -- no fake esp/rtc/wifi needed since
parser is pure. EspTestBase retains existing behavior, gains debug via
inheritance.

## Testing Strategy

### Test file

test/ConfigParserTest.cpp, deriving DebugTestBase.

### Test structure

```cpp
class ConfigParserTest : public DebugTestBase {
    ConfigParser parser{debug};
    DynamicJsonBuffer buffer{512};

    JsonObject parse(const std::string& json) {
        return buffer.parseObject(json);
    }
};
```

### Test categories

1. parseDebugEnabled: true, false, missing -> false

2. parseGlobalConfig: wifi credentials, new-style servers array, old-style
   single-server fallback, missing root -> defaults

3. parseDeviceConfig top-level: name, topics, debugPort, debugTopic, resetPin
   defaults

4. parseAnalogInputs: mcp3008 valid, missing name -> skipped, missing pins ->
   skipped, invalid type -> skipped

5. parseInterfaces per type:

| Type | Tests |
|------|-------|
| input | valid, defaults (cycle=single, debounce=10), invalid cycle -> single, missing pin -> skipped |
| output | valid, defaults (default=false, invert=false), missing pin -> skipped |
| pwm | valid, defaults (default=0, invert=false), missing pin -> skipped |
| analog | valid, internal input (""), named input, invalid named input -> skipped, defaults for max/offset/cutoff/precision/aggregate |
| encoder | valid, missing downPin/upPin -> skipped, pulse default false |
| dht | valid, dhtType variants (dht11/dht22/dht21/missing->DHT22), missing pin -> skipped |
| dallasTemperature | valid, devices default 1, missing pin -> skipped |
| hm3301 | valid, missing sda/scl -> skipped |
| counter | valid, defaults (bounceTime=0, multiplier=1.0), missing pin -> skipped |
| echo-distance | valid with triggerPin, valid without triggerPin -> reader variant, triggerTime default 10 |
| mqtt | valid, empty topic -> skipped |
| keepalive | valid, defaults (interval=10000, resetInterval=10), missing pin -> skipped |
| powerSupply | valid, defaults (pushTime=200, forceOffTime=6000, checkTime=60000, initialState=""), missing pins -> skipped |
| cover | valid, position sensors with per-sensor invert (regression test), defaults (latching=false, invertInput=false, invertOutput=false, closedPosition=0, invertPositionSensors=false), missing movement/pin -> skipped |
| status | valid (no fields) |

6. parseActions:
   - publish: valid, missing topic -> skipped, default template "%1" when
     neither payload nor template, retain/minimumSendInterval/sendDiff defaults
   - command: valid, missing target -> skipped, target not in interfaces map ->
     skipped, missing interface -> skipped
   - invalid type -> skipped

7. Cross-reference validation:
   - analog config referencing non-existent analog input -> skipped + logged
   - action referencing non-existent interface -> skipped + logged
   - command action referencing non-existent target -> skipped + logged

8. SensorConfig parsing:
   - intervalMs set -> uses it
   - intervalMs=0, interval set -> interval*1000
   - intervalMs=0, interval missing -> 60000
   - offset default 0, offset set -> offset*1000
   - pulse as string -> single element vector
   - pulse as array -> multi element vector
   - pulse missing -> empty vector

9. AnalogInputWithChannelDescription parsing:
   - "" -> {inputName="", channel=0} (internal)
   - "name.3" -> {inputName="name", channel=3}
   - "name" (no dot) -> invalid, skipped + logged
   - "name.abc" (non-numeric channel) -> invalid, skipped + logged

### Test data

Inline JSON strings via DynamicJsonBuffer::parseObject. No file I/O.

### Debug log assertions

Use LogExpectation (existing test helper) for skip/invalid cases -- verify
expected log messages emitted.

### Coverage goal

Every parser branch exercised. Factory/device code not unit tested (relies on
manual/flash testing).

## Build

CMakeLists.txt: src/common/ConfigParser.cpp added to common_sources glob (already
picks up src/common/*.cpp). No CMake change needed.

## README

Per AGENTS.md coding conventions, update README.md if any config fields change.
This refactor preserves all existing config fields and their semantics -- no
new/removed/renamed fields. README update not required.

## Out of Scope

- Operation parser refactor (already tested via OperationParserTest,
  OperationParser2Test)
- Factory/device code unit testing (relies on device-specific deps)
- Network debug setup (stays in main.cpp::setup)