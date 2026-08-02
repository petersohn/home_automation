# Config Parser Testability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `src/config.cpp` unit testable by separating pure JSON parsing from device-specific object creation. Input: config JSON. Output: config structs that tests can inspect.

**Architecture:** Three layers with strict dependency direction: (1) config structs in `src/common/` (pure data, no device deps), (2) `ConfigParser` in `src/common/` (JSON -> config structs, testable), (3) `ConfigFactory` in `src/config.cpp` (config structs -> concrete objects, device-specific). Uses `std::variant` of per-type config structs for compiler-enforced exhaustive dispatch in factory.

**Tech Stack:** C++17, GoogleTest, `clang-format`, ArduinoJson v5.13.5. Test build via `cmake` + `make` in `build/`, test binary at `build/home_automation_test`. Device build via `arduino-cli compile --fqbn esp8266:esp8266:generic --verify`. Format with `clang-format`.

**Reference spec:** `docs/superpowers/specs/2026-08-02-config-parser-testability-design.md`

---

## File Structure

### New config struct headers (`src/common/`)

Pure data, no device deps. One header per interface config, grouped only when same interface uses both.

| File | Contents |
|------|----------|
| `SensorConfig.hpp` | `SensorConfig` (interval, offset, pulse) |
| `InputConfig.hpp` | `InputConfig` + `CycleType` enum |
| `OutputConfig.hpp` | `OutputConfig` |
| `PwmConfig.hpp` | `PwmConfig` |
| `AnalogConfig.hpp` | `AnalogConfig` + `AnalogInputWithChannelDescription` |
| `AnalogInputConfig.hpp` | `AnalogInputConfig` + `Mcp3008Config` |
| `EncoderConfig.hpp` | `EncoderConfig` |
| `DhtConfig.hpp` | `DhtConfig` + DHT constants |
| `DallasTemperatureConfig.hpp` | `DallasTemperatureConfig` |
| `Hm3301Config.hpp` | `Hm3301Config` |
| `CounterConfig.hpp` | `CounterConfig` |
| `EchoDistanceConfig.hpp` | `EchoDistanceConfig` + `EchoDistanceReaderConfig` |
| `MqttInterfaceConfig.hpp` | `MqttInterfaceConfig` |
| `KeepaliveConfig.hpp` | `KeepaliveConfig` |
| `PowerSupplyConfig.hpp` | `PowerSupplyConfig` |
| `CoverConfig.hpp` | `CoverConfig` + `PositionSensor` (moved from PositionSensor.hpp) |
| `StatusConfig.hpp` | `StatusConfig` |
| `GlobalConfig.hpp` | `ServerConfig`, `GlobalConfig`, `TopicConfig`, `DeviceConfigCommon` |
| `ActionConfig.hpp` | `PublishActionConfig`, `CommandActionConfig`, `ActionConfigVariant`, `ActionEntry` |
| `InterfaceConfigs.hpp` | `InterfaceConfigVariant` + `InterfaceEntry` (aggregation, includes all per-type) |
| `ConfigParser.hpp` | `DeviceConfigDescription` + `ConfigParser` class |
| `ConfigParser.cpp` | Pure parsing implementation |

### New test files

| File | Contents |
|------|----------|
| `test/DebugTestBase.hpp` | `DebugTestBase` class (debug stream only) |
| `test/DebugTestBase.cpp` | `DebugTestBase` constructor |
| `test/ConfigParserTest.cpp` | Unit tests for `ConfigParser` |

### Modified files

| File | Change |
|------|--------|
| `test/EspTestBase.hpp` | Derive from `DebugTestBase` instead of `::testing::Test` |
| `test/EspTestBase.cpp` | Remove `debug` member init (now in base), set `debugStreambuf.esp` |
| `src/config.hpp` | `DeviceConfig` embeds `DeviceConfigCommon`; `GlobalConfig` from common |
| `src/config.cpp` | Replace `ConfigParser` class with `ConfigFactory`; use `ConfigParser` from common |
| `src/GpioInput.hpp` | Constructor takes `InputConfig`; include `common/InputConfig.hpp` for `CycleType` |
| `src/GpioInput.cpp` | Unpack `InputConfig` in constructor |
| `src/GpioOutput.hpp` | Constructor takes `OutputConfig` |
| `src/GpioOutput.cpp` | Unpack `OutputConfig` in constructor |
| `src/PwmOutput.hpp` | Constructor takes `PwmConfig` |
| `src/PwmOutput.cpp` | Unpack `PwmConfig` in constructor |
| `src/common/AnalogSensor.hpp` | Constructor takes `AnalogConfig` (timing removed) |
| `src/common/AnalogSensor.cpp` | Unpack `AnalogConfig` in constructor |
| `src/EncoderInterface.hpp` | Constructor takes `EncoderConfig` |
| `src/EncoderInterface.cpp` | Unpack `EncoderConfig` in constructor |
| `src/DhtSensor.hpp` | Include `common/DhtConfig.hpp` for DHT constants; constructor takes `DhtConfig` |
| `src/DhtSensor.cpp` | Unpack `DhtConfig` in constructor |
| `src/DallasTemperatureSensor.hpp` | Constructor takes `DallasTemperatureConfig` |
| `src/DallasTemperatureSensor.cpp` | Unpack `DallasTemperatureConfig` in constructor |
| `src/HM3301Sensor.hpp` | Constructor takes `Hm3301Config` |
| `src/HM3301Sensor.cpp` | Unpack `Hm3301Config` in constructor |
| `src/CounterInterface.hpp` | Constructor takes `CounterConfig` |
| `src/CounterInterface.cpp` | Unpack `CounterConfig` in constructor |
| `src/EchoDistanceSensor.hpp` | Constructor takes `EchoDistanceConfig` |
| `src/EchoDistanceSensor.cpp` | Unpack `EchoDistanceConfig` in constructor |
| `src/EchoDistanceReaderInterface.hpp` | Constructor takes `EchoDistanceReaderConfig` |
| `src/EchoDistanceReaderInterface.cpp` | Unpack `EchoDistanceReaderConfig` in constructor |
| `src/MqttInterface.hpp` | Constructor takes `MqttInterfaceConfig` |
| `src/MqttInterface.cpp` | Unpack `MqttInterfaceConfig` in constructor |
| `src/KeepaliveInterface.hpp` | Constructor takes `KeepaliveConfig` |
| `src/KeepaliveInterface.cpp` | Unpack `KeepaliveConfig` in constructor |
| `src/PowerSupplyInterface.hpp` | Constructor takes `PowerSupplyConfig` |
| `src/PowerSupplyInterface.cpp` | Unpack `PowerSupplyConfig` in constructor |
| `src/common/Cover.hpp` | Constructor takes `CoverConfig`; include `common/CoverConfig.hpp` |
| `src/common/Cover.cpp` | Unpack `CoverConfig` in constructor |
| `src/common/SensorInterface.hpp` | Constructor takes `SensorConfig` instead of separate interval/offset/pulse |
| `src/common/SensorInterface.cpp` | Unpack `SensorConfig` in constructor |
| `src/common/PositionSensor.hpp` | Include `common/CoverConfig.hpp` instead of defining PositionSensor |
| `src/main.cpp` | Use `deviceConfig.common.*` instead of `deviceConfig.*` for top-level fields |

---

## Execution Order

**Phase 1:** Config struct headers (pure data, no dependencies). Build still passes — headers unused.
**Phase 2:** DebugTestBase + EspTestBase refactor. Build + tests pass.
**Phase 3:** ConfigParser class (common, pure parsing). Build passes — parser unused by device code.
**Phase 4:** ConfigParser tests. Tests pass.
**Phase 5:** Device constructor changes (take config structs). Device build + tests pass.
**Phase 6:** ConfigFactory + config.cpp rewrite. Device build + tests pass.
**Phase 7:** main.cpp adaptation. Final build + tests pass.

Each phase ends with a commit. Build + test after every task.

---

## Phase 1: Config Struct Headers

### Task 1: SensorConfig header

**Files:**
- Create: `src/common/SensorConfig.hpp`

- [ ] **Step 1: Create the header**

```cpp
#ifndef SENSOR_CONFIG_HPP
#define SENSOR_CONFIG_HPP

#include <string>
#include <vector>

struct SensorConfig {
    int interval = 60000;
    int offset = 0;
    std::vector<std::string> pulse;
};

#endif  // SENSOR_CONFIG_HPP
```

- [ ] **Step 2: Verify test build**

Run: `cd build && cmake .. && make -j$(nproc) && cd ..`
Expected: PASS

- [ ] **Step 3: Commit**

```bash
git add src/common/SensorConfig.hpp
git commit -m "refactor: add SensorConfig struct header"
```

---

### Task 2: InputConfig header

**Files:**
- Create: `src/common/InputConfig.hpp`

- [ ] **Step 1: Create the header**

```cpp
#ifndef INPUT_CONFIG_HPP
#define INPUT_CONFIG_HPP

#include <cstdint>

enum class CycleType { none, single, multi };

struct InputConfig {
    uint8_t pin = 0;
    CycleType cycleType = CycleType::single;
    unsigned debounce = 10;
};

#endif  // INPUT_CONFIG_HPP
```

- [ ] **Step 2: Verify, commit**

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/InputConfig.hpp
git commit -m "refactor: add InputConfig struct + CycleType enum"
```

---

### Task 3: OutputConfig header

**Files:**
- Create: `src/common/OutputConfig.hpp`

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef OUTPUT_CONFIG_HPP
#define OUTPUT_CONFIG_HPP

#include <cstdint>

struct OutputConfig {
    uint8_t pin = 0;
    bool defaultValue = false;
    bool invert = false;
};

#endif  // OUTPUT_CONFIG_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/OutputConfig.hpp
git commit -m "refactor: add OutputConfig struct header"
```

---

### Task 4: PwmConfig header

**Files:**
- Create: `src/common/PwmConfig.hpp`

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef PWM_CONFIG_HPP
#define PWM_CONFIG_HPP

#include <cstdint>

struct PwmConfig {
    uint8_t pin = 0;
    int defaultValue = 0;
    bool invert = false;
};

#endif  // PWM_CONFIG_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/PwmConfig.hpp
git commit -m "refactor: add PwmConfig struct header"
```

---

### Task 5: AnalogInputConfig + Mcp3008Config header

**Files:**
- Create: `src/common/AnalogInputConfig.hpp`

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef ANALOG_INPUT_CONFIG_HPP
#define ANALOG_INPUT_CONFIG_HPP

#include <cstdint>
#include <string>
#include <variant>

struct Mcp3008Config {
    uint8_t sck = 0;
    uint8_t mosi = 0;
    uint8_t miso = 0;
    uint8_t cs = 0;
};

struct AnalogInputConfig {
    std::string name;
    std::variant<Mcp3008Config> input;
};

#endif  // ANALOG_INPUT_CONFIG_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/AnalogInputConfig.hpp
git commit -m "refactor: add AnalogInputConfig + Mcp3008Config headers"
```

---

### Task 6: AnalogConfig + AnalogInputWithChannelDescription header

**Files:**
- Create: `src/common/AnalogConfig.hpp`

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef ANALOG_CONFIG_HPP
#define ANALOG_CONFIG_HPP

#include <cstdint>
#include <string>

#include "SensorConfig.hpp"

struct AnalogInputWithChannelDescription {
    std::string inputName;  // "" = internal ESP analog
    uint8_t channel = 0;    // ignored for internal
};

struct AnalogConfig {
    AnalogInputWithChannelDescription input;
    double max = 0;
    double valueOffset = 0;
    double cutoff = 0;
    int precision = 0;
    unsigned aggregateTime = 0;
    unsigned aggregateDelay = 0;
    SensorConfig timing;
};

#endif  // ANALOG_CONFIG_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/AnalogConfig.hpp
git commit -m "refactor: add AnalogConfig + AnalogInputWithChannelDescription headers"
```

---

### Task 7: EncoderConfig header

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef ENCODER_CONFIG_HPP
#define ENCODER_CONFIG_HPP

#include <cstdint>

struct EncoderConfig {
    uint8_t downPin = 0;
    uint8_t upPin = 0;
    bool pulse = false;
};

#endif  // ENCODER_CONFIG_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/EncoderConfig.hpp
git commit -m "refactor: add EncoderConfig struct header"
```

---

### Task 8: DhtConfig header

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef DHT_CONFIG_HPP
#define DHT_CONFIG_HPP

#include <cstdint>

#include "SensorConfig.hpp"

constexpr int DHT_AUTO = 0;
constexpr int DHT11 = 11;
constexpr int DHT21 = 21;
constexpr int DHT22 = 22;

struct DhtConfig {
    uint8_t pin = 0;
    int type = DHT22;
    SensorConfig timing;
};

#endif  // DHT_CONFIG_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/DhtConfig.hpp
git commit -m "refactor: add DhtConfig struct + DHT constants header"
```

---

### Task 9: DallasTemperatureConfig header

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef DALLAS_TEMPERATURE_CONFIG_HPP
#define DALLAS_TEMPERATURE_CONFIG_HPP

#include <cstddef>
#include <cstdint>

#include "SensorConfig.hpp"

struct DallasTemperatureConfig {
    uint8_t pin = 0;
    std::size_t devices = 1;
    SensorConfig timing;
};

#endif  // DALLAS_TEMPERATURE_CONFIG_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/DallasTemperatureConfig.hpp
git commit -m "refactor: add DallasTemperatureConfig struct header"
```

---

### Task 10: Hm3301Config header

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef HM3301_CONFIG_HPP
#define HM3301_CONFIG_HPP

#include "SensorConfig.hpp"

struct Hm3301Config {
    int sda = 0;
    int scl = 0;
    SensorConfig timing;
};

#endif  // HM3301_CONFIG_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/Hm3301Config.hpp
git commit -m "refactor: add Hm3301Config struct header"
```

---

### Task 11: CounterConfig header

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef COUNTER_CONFIG_HPP
#define COUNTER_CONFIG_HPP

#include <cstdint>
#include <string>

#include "SensorConfig.hpp"

struct CounterConfig {
    std::string name;
    uint8_t pin = 0;
    int bounceTime = 0;
    float multiplier = 1.0f;
    SensorConfig timing;
};

#endif  // COUNTER_CONFIG_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/CounterConfig.hpp
git commit -m "refactor: add CounterConfig struct header"
```

---

### Task 12: EchoDistanceConfig header

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef ECHO_DISTANCE_CONFIG_HPP
#define ECHO_DISTANCE_CONFIG_HPP

#include <cstdint>

#include "SensorConfig.hpp"

struct EchoDistanceConfig {
    uint8_t echoPin = 0;
    uint8_t triggerPin = 0;
    unsigned triggerTime = 10;
    SensorConfig timing;
};

struct EchoDistanceReaderConfig {
    uint8_t echoPin = 0;
};

#endif  // ECHO_DISTANCE_CONFIG_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/EchoDistanceConfig.hpp
git commit -m "refactor: add EchoDistanceConfig + EchoDistanceReaderConfig headers"
```

---

### Task 13: MqttInterfaceConfig header

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef MQTT_INTERFACE_CONFIG_HPP
#define MQTT_INTERFACE_CONFIG_HPP

#include <string>

struct MqttInterfaceConfig {
    std::string topic;
};

#endif  // MQTT_INTERFACE_CONFIG_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/MqttInterfaceConfig.hpp
git commit -m "refactor: add MqttInterfaceConfig struct header"
```

---

### Task 14: KeepaliveConfig header

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef KEEPALIVE_CONFIG_HPP
#define KEEPALIVE_CONFIG_HPP

#include <cstdint>

struct KeepaliveConfig {
    uint8_t pin = 0;
    unsigned interval = 10000;
    unsigned resetInterval = 10;
};

#endif  // KEEPALIVE_CONFIG_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/KeepaliveConfig.hpp
git commit -m "refactor: add KeepaliveConfig struct header"
```

---

### Task 15: PowerSupplyConfig header

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef POWER_SUPPLY_CONFIG_HPP
#define POWER_SUPPLY_CONFIG_HPP

#include <cstdint>
#include <string>

struct PowerSupplyConfig {
    uint8_t powerSwitchPin = 0;
    uint8_t resetSwitchPin = 0;
    uint8_t powerCheckPin = 0;
    unsigned pushTime = 200;
    unsigned forceOffTime = 6000;
    unsigned checkTime = 60000;
    std::string initialState;
};

#endif  // POWER_SUPPLY_CONFIG_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/PowerSupplyConfig.hpp
git commit -m "refactor: add PowerSupplyConfig struct header"
```

---

### Task 16: CoverConfig + PositionSensor header

**Files:**
- Create: `src/common/CoverConfig.hpp`
- Modify: `src/common/PositionSensor.hpp`

- [ ] **Step 1: Create CoverConfig.hpp**

```cpp
#ifndef COVER_CONFIG_HPP
#define COVER_CONFIG_HPP

#include <cstdint>
#include <vector>

struct PositionSensor {
    int position = 0;
    uint8_t pin = 0;
    bool invert = false;
};

struct CoverConfig {
    uint8_t upMovementPin = 0;
    uint8_t downMovementPin = 0;
    uint8_t upPin = 0;
    uint8_t downPin = 0;
    uint8_t stopPin = 0;
    bool latching = false;
    bool invertInput = false;
    bool invertOutput = false;
    int closedPosition = 0;
    bool invertPositionSensors = false;
    std::vector<PositionSensor> positionSensors;
};

#endif  // COVER_CONFIG_HPP
```

- [ ] **Step 2: Update PositionSensor.hpp to forward include**

```cpp
#ifndef POSITION_SENSOR_HPP
#define POSITION_SENSOR_HPP

#include "CoverConfig.hpp"

// PositionSensor is now defined in CoverConfig.hpp.
// This header remains for backward compatibility.

#endif  // POSITION_SENSOR_HPP
```

- [ ] **Step 3: Verify + commit**

```bash
cd build && cmake .. && make -j$(nproc) && ./home_automation_test && cd ..
git add src/common/CoverConfig.hpp src/common/PositionSensor.hpp
git commit -m "refactor: add CoverConfig + move PositionSensor to CoverConfig.hpp"
```

---

### Task 17: StatusConfig header

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef STATUS_CONFIG_HPP
#define STATUS_CONFIG_HPP

struct StatusConfig {};

#endif  // STATUS_CONFIG_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/StatusConfig.hpp
git commit -m "refactor: add StatusConfig struct header"
```

---

### Task 18: GlobalConfig header

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef GLOBAL_CONFIG_HPP
#define GLOBAL_CONFIG_HPP

#include <cstdint>
#include <string>
#include <vector>

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

#endif  // GLOBAL_CONFIG_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/GlobalConfig.hpp
git commit -m "refactor: add GlobalConfig + DeviceConfigCommon headers"
```

---

### Task 19: ActionConfig header

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef ACTION_CONFIG_HPP
#define ACTION_CONFIG_HPP

#include <string>
#include <variant>

struct PublishActionConfig {
    std::string topic;
    std::string operationJson;
    bool retain = false;
    unsigned minimumSendInterval = 0;
    double sendDiff = 0.0;
};

struct CommandActionConfig {
    std::string target;
    std::string operationJson;
};

using ActionConfigVariant = std::variant<PublishActionConfig, CommandActionConfig>;

struct ActionEntry {
    std::string interface;
    ActionConfigVariant config;
};

#endif  // ACTION_CONFIG_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/ActionConfig.hpp
git commit -m "refactor: add ActionConfig struct headers"
```

---

### Task 20: InterfaceConfigs aggregation header

- [ ] **Step 1: Create + verify + commit**

```cpp
#ifndef INTERFACE_CONFIGS_HPP
#define INTERFACE_CONFIGS_HPP

#include <string>
#include <variant>

#include "AnalogConfig.hpp"
#include "CounterConfig.hpp"
#include "CoverConfig.hpp"
#include "DallasTemperatureConfig.hpp"
#include "DhtConfig.hpp"
#include "EchoDistanceConfig.hpp"
#include "EncoderConfig.hpp"
#include "Hm3301Config.hpp"
#include "InputConfig.hpp"
#include "KeepaliveConfig.hpp"
#include "MqttInterfaceConfig.hpp"
#include "OutputConfig.hpp"
#include "PowerSupplyConfig.hpp"
#include "PwmConfig.hpp"
#include "StatusConfig.hpp"

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

#endif  // INTERFACE_CONFIGS_HPP
```

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/InterfaceConfigs.hpp
git commit -m "refactor: add InterfaceConfigs aggregation header with variant"
```

---

## Phase 2: Test Base Refactor

### Task 21: DebugTestBase + EspTestBase refactor

**Files:**
- Create: `test/DebugTestBase.hpp`
- Create: `test/DebugTestBase.cpp`
- Modify: `test/EspTestBase.hpp`
- Modify: `test/EspTestBase.cpp`

- [ ] **Step 1: Create DebugTestBase.hpp**

```cpp
#ifndef TEST_DEBUGTESTBASE_HPP
#define TEST_DEBUGTESTBASE_HPP

#include <gtest/gtest.h>

#include <ostream>

#include "TestStream.hpp"

class DebugTestBase : public ::testing::Test {
public:
    TestStreambuf debugStreambuf;
    std::ostream debug;
    DebugTestBase();
};

#endif  // TEST_DEBUGTESTBASE_HPP
```

- [ ] **Step 2: Create DebugTestBase.cpp**

```cpp
#include "DebugTestBase.hpp"

DebugTestBase::DebugTestBase() : debug(&this->debugStreambuf) {}
```

- [ ] **Step 3: Update EspTestBase.hpp**

Replace `TestStreambuf debugStreambuf` and `std::ostream debug` with inheritance. Full new file:

```cpp
#ifndef TEST_ESPTESTBASE_HPP
#define TEST_ESPTESTBASE_HPP

#include <gtest/gtest.h>

#include <functional>

#include "DebugTestBase.hpp"
#include "FakeEspApi.hpp"
#include "FakeRtc.hpp"
#include "FakeWifi.hpp"

#define ASSERT_NO_FAILURE()                  \
    do {                                     \
        if (::testing::Test::HasFailure()) { \
            FAIL();                          \
        }                                    \
    } while (false)

class LogExpectation;

class EspTestBase : public DebugTestBase {
public:
    FakeEspApi esp;
    FakeRtc rtc;
    FakeWifi wifi;

    EspTestBase();
    ~EspTestBase();

    void delayUntil(
        unsigned long time, unsigned long delay, std::function<void()> func);
    std::shared_ptr<LogExpectation> expectLog(
        std::string log, size_t count = 1);
};

#endif  // TEST_ESPTESTBASE_HPP
```

- [ ] **Step 4: Update EspTestBase.cpp**

```cpp
#include "EspTestBase.hpp"

#include "LogExpectation.hpp"

EspTestBase::EspTestBase() {
    this->debugStreambuf.esp = &this->esp;
}

EspTestBase::~EspTestBase() {
    this->debugStreambuf.esp = nullptr;
}

void EspTestBase::delayUntil(
    unsigned long time, unsigned long delay, std::function<void()> func) {
    while (this->esp.millis() < time) {
        this->esp.delay(std::min(delay, time - this->esp.millis()));
        func();
    }
}

std::shared_ptr<LogExpectation> EspTestBase::expectLog(
    std::string log, size_t count) {
    auto expectation = std::make_shared<LogExpectation>(log, count);
    this->debugStreambuf.addExpectation(expectation);
    return expectation;
}
```

- [ ] **Step 5: Build and run all tests**

Run: `cd build && cmake .. && make -j$(nproc) && ./home_automation_test`
Expected: All existing tests PASS

- [ ] **Step 6: Commit**

```bash
git add test/DebugTestBase.hpp test/DebugTestBase.cpp test/EspTestBase.hpp test/EspTestBase.cpp
git commit -m "refactor: extract DebugTestBase from EspTestBase"
```

---

## Phase 3: ConfigParser Class

### Task 22: ConfigParser header

**Files:**
- Create: `src/common/ConfigParser.hpp`

- [ ] **Step 1: Create the header**

Template helper methods are defined inline to avoid linker issues. The header includes all config struct headers it needs.

```cpp
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
    DeviceConfigDescription parseDeviceConfig(const ArduinoJson::JsonObject& root);

private:
    std::ostream& debug;

    template <typename T>
    bool getRequiredValue(const ArduinoJson::JsonObject& data, const char* name, T& value) {
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

    std::optional<InterfaceConfigVariant> parseInterface(const ArduinoJson::JsonObject& data);
    std::optional<InputConfig> parseInput(const ArduinoJson::JsonObject& data);
    std::optional<OutputConfig> parseOutput(const ArduinoJson::JsonObject& data);
    std::optional<PwmConfig> parsePwm(const ArduinoJson::JsonObject& data);
    std::optional<AnalogConfig> parseAnalog(const ArduinoJson::JsonObject& data);
    std::optional<EncoderConfig> parseEncoder(const ArduinoJson::JsonObject& data);
    std::optional<DhtConfig> parseDht(const ArduinoJson::JsonObject& data);
    std::optional<DallasTemperatureConfig> parseDallasTemperature(const ArduinoJson::JsonObject& data);
    std::optional<Hm3301Config> parseHm3301(const ArduinoJson::JsonObject& data);
    std::optional<CounterConfig> parseCounter(const ArduinoJson::JsonObject& data);
    std::optional<InterfaceConfigVariant> parseEchoDistance(const ArduinoJson::JsonObject& data);
    std::optional<MqttInterfaceConfig> parseMqttInterface(const ArduinoJson::JsonObject& data);
    std::optional<KeepaliveConfig> parseKeepalive(const ArduinoJson::JsonObject& data);
    std::optional<PowerSupplyConfig> parsePowerSupply(const ArduinoJson::JsonObject& data);
    std::optional<CoverConfig> parseCover(const ArduinoJson::JsonObject& data);
    std::optional<StatusConfig> parseStatus(const ArduinoJson::JsonObject& data);

    std::optional<AnalogInputConfig> parseAnalogInput(const ArduinoJson::JsonObject& data);
    std::optional<AnalogInputWithChannelDescription> parseAnalogInputWithChannel(const std::string& value);

    std::unordered_map<std::string, InterfaceEntry> parseInterfaces(
        const ArduinoJson::JsonObject& data,
        const std::unordered_map<std::string, AnalogInputConfig>& analogInputs);
    std::vector<ActionEntry> parseActions(
        const ArduinoJson::JsonObject& data,
        const std::unordered_map<std::string, InterfaceEntry>& interfaces);
    std::optional<ActionConfigVariant> parseAction(const ArduinoJson::JsonObject& data);

    std::string serializeOperationJson(const ArduinoJson::JsonObject& data);
};

#endif  // COMMON_CONFIG_PARSER_HPP
```

- [ ] **Step 2: Verify build**

Run: `cd build && cmake .. && make -j$(nproc) && cd ..`
Expected: PASS

- [ ] **Step 3: Commit**

```bash
git add src/common/ConfigParser.hpp
git commit -m "refactor: add ConfigParser class header"
```

---

### Task 23: ConfigParser implementation — helpers + global config + debug

**Files:**
- Create: `src/common/ConfigParser.cpp`

- [ ] **Step 1: Create ConfigParser.cpp**

```cpp
#include "ConfigParser.hpp"

#include <algorithm>
#include <memory>

#include "common/ArduinoJson.hpp"
#include "tools/collection.hpp"

using namespace ArduinoJson;

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
    {"", DHT22}, {"dht11", DHT11}, {"dht22", DHT22}, {"dht21", DHT21}};

int ConfigParser::getDhtType(const std::string& value) {
    auto type = tools::findValue(dhtTypes, value);
    return type ? *type : DHT22;
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

}  // unnamed namespace

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

    JsonArray& servers = root.get<JsonVariant>("servers");
    if (servers == JsonArray::invalid()) {
        this->debug << "No servers config. "
                       "Attempting old-style single-server config."
                    << std::endl;
        result.servers.push_back(parseSingleServerConfig(root));
    } else {
        for (auto server : servers) {
            result.servers.push_back(parseServerConfig(server.as<JsonObject>()));
        }
    }

    return result;
}
```

- [ ] **Step 2: Build + commit**

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/ConfigParser.cpp
git commit -m "refactor: add ConfigParser helpers + parseGlobalConfig + parseDebugEnabled"
```

---

### Task 24: ConfigParser — per-type interface parsers

**Files:**
- Modify: `src/common/ConfigParser.cpp`

- [ ] **Step 1: Add all per-type parsers + parseInterface dispatch**

Append to `src/common/ConfigParser.cpp`:

```cpp
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

std::optional<AnalogInputWithChannelDescription> ConfigParser::parseAnalogInputWithChannel(
    const std::string& value) {
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
        this->debug << "Not a valid channel number: " << channelStr << std::endl;
        return std::nullopt;
    }

    AnalogInputWithChannelDescription result;
    result.inputName = name;
    result.channel = v.as<uint8_t>();
    return result;
}

std::optional<AnalogConfig> ConfigParser::parseAnalog(const JsonObject& data) {
    auto inputDesc = parseAnalogInputWithChannel(data["input"].as<std::string>());
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

std::optional<EncoderConfig> ConfigParser::parseEncoder(const JsonObject& data) {
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

std::optional<CounterConfig> ConfigParser::parseCounter(const JsonObject& data) {
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

    uint8_t triggerPin = getJsonWithDefault(data["triggerPin"], static_cast<uint8_t>(0));
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
    result.initialState = getJsonWithDefault(data["initialState"], std::string(""));
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
    result.stopPin = getJsonWithDefault(data["stopPin"], static_cast<uint8_t>(0));
    result.latching = getJsonWithDefault(data["latching"], false);
    result.invertInput = getJsonWithDefault(data["invertInput"], false);
    result.invertOutput = getJsonWithDefault(data["invertOutput"], false);
    result.closedPosition = getJsonWithDefault(data["closedPosition"], 0);
    result.invertPositionSensors = getJsonWithDefault(data["invertPositionSensors"], false);
    return result;
}

std::optional<StatusConfig> ConfigParser::parseStatus(const JsonObject& data) {
    return StatusConfig{};
}

std::optional<InterfaceConfigVariant> ConfigParser::parseInterface(
    const JsonObject& data) {
    std::string type = data.get<std::string>("type");
    if (type == "input") {
        auto r = parseInput(data);
        return r ? std::optional<InterfaceConfigVariant>(*r) : std::nullopt;
    } else if (type == "output") {
        auto r = parseOutput(data);
        return r ? std::optional<InterfaceConfigVariant>(*r) : std::nullopt;
    } else if (type == "pwm") {
        auto r = parsePwm(data);
        return r ? std::optional<InterfaceConfigVariant>(*r) : std::nullopt;
    } else if (type == "analog") {
        auto r = parseAnalog(data);
        return r ? std::optional<InterfaceConfigVariant>(*r) : std::nullopt;
    } else if (type == "encoder") {
        auto r = parseEncoder(data);
        return r ? std::optional<InterfaceConfigVariant>(*r) : std::nullopt;
    } else if (type == "dht") {
        auto r = parseDht(data);
        return r ? std::optional<InterfaceConfigVariant>(*r) : std::nullopt;
    } else if (type == "dallasTemperature") {
        auto r = parseDallasTemperature(data);
        return r ? std::optional<InterfaceConfigVariant>(*r) : std::nullopt;
    } else if (type == "hm3301") {
        auto r = parseHm3301(data);
        return r ? std::optional<InterfaceConfigVariant>(*r) : std::nullopt;
    } else if (type == "counter") {
        auto r = parseCounter(data);
        return r ? std::optional<InterfaceConfigVariant>(*r) : std::nullopt;
    } else if (type == "hc-sr04" || type == "echo-distance") {
        return parseEchoDistance(data);
    } else if (type == "mqtt") {
        auto r = parseMqttInterface(data);
        return r ? std::optional<InterfaceConfigVariant>(*r) : std::nullopt;
    } else if (type == "keepalive") {
        auto r = parseKeepalive(data);
        return r ? std::optional<InterfaceConfigVariant>(*r) : std::nullopt;
    } else if (type == "powerSupply") {
        auto r = parsePowerSupply(data);
        return r ? std::optional<InterfaceConfigVariant>(*r) : std::nullopt;
    } else if (type == "cover") {
        auto r = parseCover(data);
        return r ? std::optional<InterfaceConfigVariant>(*r) : std::nullopt;
    } else if (type == "status") {
        auto r = parseStatus(data);
        return r ? std::optional<InterfaceConfigVariant>(*r) : std::nullopt;
    }
    this->debug << "Invalid interface type: " << type << std::endl;
    return std::nullopt;
}
```

- [ ] **Step 2: Build + commit**

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/ConfigParser.cpp
git commit -m "refactor: add per-type interface parsers to ConfigParser"
```

---

### Task 25: ConfigParser — analog inputs, interfaces, actions, device config

**Files:**
- Modify: `src/common/ConfigParser.cpp`

- [ ] **Step 1: Add analog input, interface, action, device config parsers**

Append to `src/common/ConfigParser.cpp`:

```cpp
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
                analogInputs.find(analogCfg.input.inputName) == analogInputs.end()) {
                this->debug << "Analog input not found: " << analogCfg.input.inputName
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
            this->debug << "Interface not found: " << interfaceName << std::endl;
            continue;
        }

        auto type = action.get<std::string>("type");
        if (type == "command") {
            const std::string targetName = action["target"];
            if (interfaces.find(targetName) == interfaces.end()) {
                this->debug << "Interface not found: " << targetName << std::endl;
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

DeviceConfigDescription ConfigParser::parseDeviceConfig(const JsonObject& root) {
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
    result.common.resetPin = getJsonWithDefault(root["resetPin"], static_cast<uint8_t>(255));

    {
        const JsonArray& inputs = root["analogInputs"];
        if (inputs != JsonArray::invalid()) {
            for (const JsonObject& input : inputs) {
                if (input == JsonObject::invalid()) {
                    this->debug << "Input configuration must be an object." << std::endl;
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
```

- [ ] **Step 2: Build + commit**

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
git add src/common/ConfigParser.cpp
git commit -m "refactor: add parseDeviceConfig + analog inputs + actions to ConfigParser"
```

---

## Phase 4: ConfigParser Tests

### Task 26: Test skeleton + parseDebugEnabled + parseGlobalConfig tests

**Files:**
- Create: `test/ConfigParserTest.cpp`

- [ ] **Step 1: Create test file**

```cpp
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

    JsonObject parse(const std::string& json) {
        return buffer.parseObject(json);
    }
};

TEST_F(ConfigParserTest, ParseDebugEnabled_True) {
    auto root = parse(R"({"debug": true})");
    EXPECT_TRUE(parser.parseDebugEnabled(root));
}

TEST_F(ConfigParserTest, ParseDebugEnabled_False) {
    auto root = parse(R"({"debug": false})");
    EXPECT_FALSE(parser.parseDebugEnabled(root));
}

TEST_F(ConfigParserTest, ParseDebugEnabled_Missing) {
    auto root = parse(R"({})");
    EXPECT_FALSE(parser.parseDebugEnabled(root));
}

TEST_F(ConfigParserTest, ParseGlobalConfig_NewStyleServers) {
    auto root = parse(R"({
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
    auto root = parse(R"({
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
    auto root = parse(R"({})");
    auto result = parser.parseGlobalConfig(root);
    EXPECT_EQ(result.wifiSSID, "");
    ASSERT_EQ(result.servers.size(), 1u);
    EXPECT_EQ(result.servers[0].address, "");
}
```

- [ ] **Step 2: Build + run + commit**

```bash
cd build && cmake .. && make -j$(nproc) && ./home_automation_test --gtest_filter=ConfigParserTest.*
git add test/ConfigParserTest.cpp
git commit -m "test: add ConfigParserTest skeleton + parseDebugEnabled + parseGlobalConfig tests"
```

---

### Task 27: parseDeviceConfig top-level + interface type tests

**Files:**
- Modify: `test/ConfigParserTest.cpp`

- [ ] **Step 1: Add top-level + all interface type tests**

Append to `test/ConfigParserTest.cpp`. These tests cover: top-level fields, defaults, every interface type (valid + invalid + defaults), SensorConfig, AnalogInputWithChannelDescription, analog inputs, cross-reference validation, and actions.

```cpp
// --- Top-level ---

TEST_F(ConfigParserTest, ParseDeviceConfig_TopLevelFields) {
    auto root = parse(R"({
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
    auto root = parse(R"({})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.common.debugPort, 2534);
    EXPECT_EQ(result.common.resetPin, 255);
    EXPECT_FALSE(result.common.debug);
    EXPECT_TRUE(result.interfaces.empty());
}

// --- input ---

TEST_F(ConfigParserTest, ParseInput_Valid) {
    auto root = parse(R"({"interfaces":[{"type":"input","name":"btn","cycle":"single","pin":0,"debounce":50}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<InputConfig>(result.interfaces.at("btn").config);
    EXPECT_EQ(cfg.pin, 0);
    EXPECT_EQ(cfg.cycleType, CycleType::single);
    EXPECT_EQ(cfg.debounce, 50u);
}

TEST_F(ConfigParserTest, ParseInput_Defaults) {
    auto root = parse(R"({"interfaces":[{"type":"input","name":"btn","pin":3}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<InputConfig>(result.interfaces.at("btn").config);
    EXPECT_EQ(cfg.cycleType, CycleType::single);
    EXPECT_EQ(cfg.debounce, 10u);
}

TEST_F(ConfigParserTest, ParseInput_MissingPin_Skipped) {
    auto root = parse(R"({"interfaces":[{"type":"input","name":"btn"}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

// --- output ---

TEST_F(ConfigParserTest, ParseOutput_Valid) {
    auto root = parse(R"({"interfaces":[{"type":"output","name":"led","pin":2,"default":true,"invert":true}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<OutputConfig>(result.interfaces.at("led").config);
    EXPECT_EQ(cfg.pin, 2);
    EXPECT_TRUE(cfg.defaultValue);
    EXPECT_TRUE(cfg.invert);
}

TEST_F(ConfigParserTest, ParseOutput_Defaults) {
    auto root = parse(R"({"interfaces":[{"type":"output","name":"led","pin":2}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<OutputConfig>(result.interfaces.at("led").config);
    EXPECT_FALSE(cfg.defaultValue);
    EXPECT_FALSE(cfg.invert);
}

// --- pwm ---

TEST_F(ConfigParserTest, ParsePwm_Valid) {
    auto root = parse(R"({"interfaces":[{"type":"pwm","name":"dim","pin":4,"default":128,"invert":true}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<PwmConfig>(result.interfaces.at("dim").config);
    EXPECT_EQ(cfg.pin, 4);
    EXPECT_EQ(cfg.defaultValue, 128);
    EXPECT_TRUE(cfg.invert);
}

TEST_F(ConfigParserTest, ParsePwm_Defaults) {
    auto root = parse(R"({"interfaces":[{"type":"pwm","name":"dim","pin":4}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<PwmConfig>(result.interfaces.at("dim").config);
    EXPECT_EQ(cfg.defaultValue, 0);
    EXPECT_FALSE(cfg.invert);
}

// --- analog ---

TEST_F(ConfigParserTest, ParseAnalog_InternalInput) {
    auto root = parse(R"({"interfaces":[{"type":"analog","name":"s","input":"","max":100.0,"precision":2}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<AnalogConfig>(result.interfaces.at("s").config);
    EXPECT_EQ(cfg.input.inputName, "");
    EXPECT_EQ(cfg.max, 100.0);
    EXPECT_EQ(cfg.precision, 2);
}

TEST_F(ConfigParserTest, ParseAnalog_NamedInput) {
    auto root = parse(R"({
        "analogInputs":[{"name":"mcp","type":"mcp3008","sck":1,"mosi":2,"miso":3,"cs":4}],
        "interfaces":[{"type":"analog","name":"s","input":"mcp.5"}]
    })");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<AnalogConfig>(result.interfaces.at("s").config);
    EXPECT_EQ(cfg.input.inputName, "mcp");
    EXPECT_EQ(cfg.input.channel, 5);
}

TEST_F(ConfigParserTest, ParseAnalog_InvalidNamedInput_Skipped) {
    auto root = parse(R"({"interfaces":[{"type":"analog","name":"s","input":"nonexistent.0"}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

// --- encoder ---

TEST_F(ConfigParserTest, ParseEncoder_Valid) {
    auto root = parse(R"({"interfaces":[{"type":"encoder","name":"enc","downPin":1,"upPin":2,"pulse":true}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<EncoderConfig>(result.interfaces.at("enc").config);
    EXPECT_EQ(cfg.downPin, 1);
    EXPECT_EQ(cfg.upPin, 2);
    EXPECT_TRUE(cfg.pulse);
}

TEST_F(ConfigParserTest, ParseEncoder_MissingPins_Skipped) {
    auto root = parse(R"({"interfaces":[{"type":"encoder","name":"enc"}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

// --- dht ---

TEST_F(ConfigParserTest, ParseDht_Dht11) {
    auto root = parse(R"({"interfaces":[{"type":"dht","name":"t","pin":5,"dhtType":"dht11"}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    EXPECT_EQ(cfg.pin, 5);
    EXPECT_EQ(cfg.type, DHT11);
}

TEST_F(ConfigParserTest, ParseDht_DefaultDht22) {
    auto root = parse(R"({"interfaces":[{"type":"dht","name":"t","pin":5}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    EXPECT_EQ(cfg.type, DHT22);
}

// --- dallasTemperature ---

TEST_F(ConfigParserTest, ParseDallasTemperature_Valid) {
    auto root = parse(R"({"interfaces":[{"type":"dallasTemperature","name":"ds","pin":4,"devices":3}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DallasTemperatureConfig>(result.interfaces.at("ds").config);
    EXPECT_EQ(cfg.pin, 4);
    EXPECT_EQ(cfg.devices, 3u);
}

TEST_F(ConfigParserTest, ParseDallasTemperature_DefaultDevices) {
    auto root = parse(R"({"interfaces":[{"type":"dallasTemperature","name":"ds","pin":4}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DallasTemperatureConfig>(result.interfaces.at("ds").config);
    EXPECT_EQ(cfg.devices, 1u);
}

// --- hm3301 ---

TEST_F(ConfigParserTest, ParseHm3301_Valid) {
    auto root = parse(R"({"interfaces":[{"type":"hm3301","name":"dust","sda":1,"scl":2}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<Hm3301Config>(result.interfaces.at("dust").config);
    EXPECT_EQ(cfg.sda, 1);
    EXPECT_EQ(cfg.scl, 2);
}

TEST_F(ConfigParserTest, ParseHm3301_MissingPins_Skipped) {
    auto root = parse(R"({"interfaces":[{"type":"hm3301","name":"dust"}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

// --- counter ---

TEST_F(ConfigParserTest, ParseCounter_Valid) {
    auto root = parse(R"({"interfaces":[{"type":"counter","name":"rain","pin":0,"multiplier":1800,"bounceTime":50}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<CounterConfig>(result.interfaces.at("rain").config);
    EXPECT_EQ(cfg.name, "rain");
    EXPECT_EQ(cfg.pin, 0);
    EXPECT_EQ(cfg.multiplier, 1800.0f);
    EXPECT_EQ(cfg.bounceTime, 50);
}

TEST_F(ConfigParserTest, ParseCounter_Defaults) {
    auto root = parse(R"({"interfaces":[{"type":"counter","name":"cnt","pin":5}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<CounterConfig>(result.interfaces.at("cnt").config);
    EXPECT_EQ(cfg.multiplier, 1.0f);
    EXPECT_EQ(cfg.bounceTime, 0);
}

// --- echo-distance ---

TEST_F(ConfigParserTest, ParseEchoDistance_WithTrigger) {
    auto root = parse(R"({"interfaces":[{"type":"echo-distance","name":"dist","echoPin":1,"triggerPin":2,"triggerTime":20}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<EchoDistanceConfig>(result.interfaces.at("dist").config);
    EXPECT_EQ(cfg.echoPin, 1);
    EXPECT_EQ(cfg.triggerPin, 2);
    EXPECT_EQ(cfg.triggerTime, 20u);
}

TEST_F(ConfigParserTest, ParseEchoDistance_WithoutTrigger_ReaderVariant) {
    auto root = parse(R"({"interfaces":[{"type":"echo-distance","name":"dist","echoPin":3}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<EchoDistanceReaderConfig>(result.interfaces.at("dist").config);
    EXPECT_EQ(cfg.echoPin, 3);
}

// --- mqtt ---

TEST_F(ConfigParserTest, ParseMqttInterface_Valid) {
    auto root = parse(R"({"interfaces":[{"type":"mqtt","name":"mir","topic":"cmnd/topic"}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<MqttInterfaceConfig>(result.interfaces.at("mir").config);
    EXPECT_EQ(cfg.topic, "cmnd/topic");
}

TEST_F(ConfigParserTest, ParseMqttInterface_EmptyTopic_Skipped) {
    auto root = parse(R"({"interfaces":[{"type":"mqtt","name":"mir","topic":""}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

// --- keepalive ---

TEST_F(ConfigParserTest, ParseKeepalive_Valid) {
    auto root = parse(R"({"interfaces":[{"type":"keepalive","name":"ka","pin":6,"interval":5000,"resetInterval":20}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<KeepaliveConfig>(result.interfaces.at("ka").config);
    EXPECT_EQ(cfg.pin, 6);
    EXPECT_EQ(cfg.interval, 5000u);
    EXPECT_EQ(cfg.resetInterval, 20u);
}

TEST_F(ConfigParserTest, ParseKeepalive_Defaults) {
    auto root = parse(R"({"interfaces":[{"type":"keepalive","name":"ka","pin":6}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<KeepaliveConfig>(result.interfaces.at("ka").config);
    EXPECT_EQ(cfg.interval, 10000u);
    EXPECT_EQ(cfg.resetInterval, 10u);
}

// --- powerSupply ---

TEST_F(ConfigParserTest, ParsePowerSupply_Valid) {
    auto root = parse(R"({"interfaces":[{"type":"powerSupply","name":"ps","powerSwitchPin":1,"resetSwitchPin":2,"powerCheckPin":3,"pushTime":100,"forceOffTime":5000,"checkTime":30000,"initialState":"on"}]})");
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
    auto root = parse(R"({"interfaces":[{"type":"powerSupply","name":"ps","powerSwitchPin":1,"resetSwitchPin":2,"powerCheckPin":3}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<PowerSupplyConfig>(result.interfaces.at("ps").config);
    EXPECT_EQ(cfg.pushTime, 200u);
    EXPECT_EQ(cfg.forceOffTime, 6000u);
    EXPECT_EQ(cfg.checkTime, 60000u);
    EXPECT_EQ(cfg.initialState, "");
}

TEST_F(ConfigParserTest, ParsePowerSupply_MissingPins_Skipped) {
    auto root = parse(R"({"interfaces":[{"type":"powerSupply","name":"ps","powerSwitchPin":1}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

// --- cover ---

TEST_F(ConfigParserTest, ParseCover_Valid) {
    auto root = parse(R"({
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

TEST_F(ConfigParserTest, ParseCover_PerSensorInvert_Regression) {
    auto root = parse(R"({
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
    auto root = parse(R"({"interfaces":[{"type":"cover","name":"gate","upMovementPin":1,"downMovementPin":2,"upPin":3,"downPin":4}]})");
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
    auto root = parse(R"({"interfaces":[{"type":"cover","name":"gate","upMovementPin":1}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

// --- status ---

TEST_F(ConfigParserTest, ParseStatus_Valid) {
    auto root = parse(R"({"interfaces":[{"type":"status","name":"stat"}]})");
    auto result = parser.parseDeviceConfig(root);
    ASSERT_EQ(result.interfaces.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<StatusConfig>(result.interfaces.at("stat").config));
}

// --- invalid type + commandTopic ---

TEST_F(ConfigParserTest, ParseInterface_InvalidType_Skipped) {
    auto root = parse(R"({"interfaces":[{"type":"nonexistent","name":"x"}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

TEST_F(ConfigParserTest, ParseInterface_CommandTopic) {
    auto root = parse(R"({"interfaces":[{"type":"output","name":"led","pin":2,"commandTopic":"dev/test/led/set"}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.at("led").commandTopic, "dev/test/led/set");
}

// --- SensorConfig ---

TEST_F(ConfigParserTest, SensorConfig_IntervalMs) {
    auto root = parse(R"({"interfaces":[{"type":"dht","name":"t","pin":0,"intervalMs":5000}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    EXPECT_EQ(cfg.timing.interval, 5000);
}

TEST_F(ConfigParserTest, SensorConfig_IntervalSeconds) {
    auto root = parse(R"({"interfaces":[{"type":"dht","name":"t","pin":0,"interval":10}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    EXPECT_EQ(cfg.timing.interval, 10000);
}

TEST_F(ConfigParserTest, SensorConfig_IntervalDefault) {
    auto root = parse(R"({"interfaces":[{"type":"dht","name":"t","pin":0}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    EXPECT_EQ(cfg.timing.interval, 60000);
}

TEST_F(ConfigParserTest, SensorConfig_Offset) {
    auto root = parse(R"({"interfaces":[{"type":"dht","name":"t","pin":0,"offset":5}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    EXPECT_EQ(cfg.timing.offset, 5000);
}

TEST_F(ConfigParserTest, SensorConfig_PulseString) {
    auto root = parse(R"({"interfaces":[{"type":"dht","name":"t","pin":0,"pulse":"on"}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    ASSERT_EQ(cfg.timing.pulse.size(), 1u);
    EXPECT_EQ(cfg.timing.pulse[0], "on");
}

TEST_F(ConfigParserTest, SensorConfig_PulseArray) {
    auto root = parse(R"({"interfaces":[{"type":"dht","name":"t","pin":0,"pulse":["a","b","c"]}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    ASSERT_EQ(cfg.timing.pulse.size(), 3u);
}

TEST_F(ConfigParserTest, SensorConfig_PulseMissing) {
    auto root = parse(R"({"interfaces":[{"type":"dht","name":"t","pin":0}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<DhtConfig>(result.interfaces.at("t").config);
    EXPECT_TRUE(cfg.timing.pulse.empty());
}

// --- AnalogInputWithChannelDescription ---

TEST_F(ConfigParserTest, AnalogInputWithChannel_Internal) {
    auto root = parse(R"({"interfaces":[{"type":"analog","name":"s","input":""}]})");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<AnalogConfig>(result.interfaces.at("s").config);
    EXPECT_EQ(cfg.input.inputName, "");
    EXPECT_EQ(cfg.input.channel, 0);
}

TEST_F(ConfigParserTest, AnalogInputWithChannel_Named) {
    auto root = parse(R"({
        "analogInputs":[{"name":"mcp","type":"mcp3008","sck":1,"mosi":2,"miso":3,"cs":4}],
        "interfaces":[{"type":"analog","name":"s","input":"mcp.3"}]
    })");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<AnalogConfig>(result.interfaces.at("s").config);
    EXPECT_EQ(cfg.input.inputName, "mcp");
    EXPECT_EQ(cfg.input.channel, 3);
}

// --- Analog inputs ---

TEST_F(ConfigParserTest, ParseAnalogInputs_Mcp3008) {
    auto root = parse(R"({"analogInputs":[{"name":"mcp","type":"mcp3008","sck":1,"mosi":2,"miso":3,"cs":4}]})");
    auto result = parser.parseDeviceConfig(root);
    ASSERT_EQ(result.analogInputs.size(), 1u);
    auto& mcp = std::get<Mcp3008Config>(result.analogInputs.at("mcp").input);
    EXPECT_EQ(mcp.sck, 1);
    EXPECT_EQ(mcp.mosi, 2);
    EXPECT_EQ(mcp.miso, 3);
    EXPECT_EQ(mcp.cs, 4);
}

TEST_F(ConfigParserTest, ParseAnalogInputs_MissingName_Skipped) {
    auto root = parse(R"({"analogInputs":[{"type":"mcp3008","sck":1,"mosi":2,"miso":3,"cs":4}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.analogInputs.size(), 0u);
}

TEST_F(ConfigParserTest, ParseAnalogInputs_InvalidType_Skipped) {
    auto root = parse(R"({"analogInputs":[{"name":"bad","type":"unknown"}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.analogInputs.size(), 0u);
}

// --- Cross-reference validation ---

TEST_F(ConfigParserTest, CrossRef_AnalogInputNotFound_Skipped) {
    auto root = parse(R"({"interfaces":[{"type":"analog","name":"s","input":"nonexistent.0"}]})");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.interfaces.size(), 0u);
}

TEST_F(ConfigParserTest, CrossRef_ActionInterfaceNotFound_Skipped) {
    auto root = parse(R"({
        "interfaces":[{"type":"input","name":"btn","pin":0}],
        "actions":[{"type":"publish","interface":"nonexistent","topic":"t"}]
    })");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.actions.size(), 0u);
}

TEST_F(ConfigParserTest, CrossRef_CommandTargetNotFound_Skipped) {
    auto root = parse(R"({
        "interfaces":[{"type":"input","name":"btn","pin":0}],
        "actions":[{"type":"command","interface":"btn","target":"nonexistent","command":"toggle"}]
    })");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.actions.size(), 0u);
}

// --- Actions ---

TEST_F(ConfigParserTest, ParseActions_PublishValid) {
    auto root = parse(R"({
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
    auto root = parse(R"({
        "interfaces":[{"type":"input","name":"btn","pin":0}],
        "actions":[{"type":"publish","interface":"btn"}]
    })");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.actions.size(), 0u);
}

TEST_F(ConfigParserTest, ParseActions_PublishDefaultTemplate) {
    auto root = parse(R"({
        "interfaces":[{"type":"input","name":"btn","pin":0}],
        "actions":[{"type":"publish","interface":"btn","topic":"t"}]
    })");
    auto result = parser.parseDeviceConfig(root);
    auto& cfg = std::get<PublishActionConfig>(result.actions[0].config);
    EXPECT_NE(cfg.operationJson.find("%1"), std::string::npos);
}

TEST_F(ConfigParserTest, ParseActions_CommandValid) {
    auto root = parse(R"({
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
    auto root = parse(R"({
        "interfaces":[{"type":"input","name":"btn","pin":0}],
        "actions":[{"type":"invalid","interface":"btn"}]
    })");
    auto result = parser.parseDeviceConfig(root);
    EXPECT_EQ(result.actions.size(), 0u);
}
```

- [ ] **Step 2: Build + run + commit**

```bash
cd build && cmake .. && make -j$(nproc) && ./home_automation_test --gtest_filter=ConfigParserTest.*
git add test/ConfigParserTest.cpp
git commit -m "test: add all ConfigParser tests (interfaces, sensors, actions, cross-ref)"
```

---

## Phase 5: Device Constructor Changes

### Task 28: GpioInput takes InputConfig

**Files:**
- Modify: `src/GpioInput.hpp`
- Modify: `src/GpioInput.cpp`

- [ ] **Step 1: Update GpioInput.hpp**

Remove the `CycleType` enum (now in `common/InputConfig.hpp`). Include `common/InputConfig.hpp`. Change constructor to take `const InputConfig& config`.

- [ ] **Step 2: Update GpioInput.cpp constructor**

Unpack `config.pin`, `config.cycleType`, `config.debounce` into member init list.

- [ ] **Step 3: Verify builds + format + commit**

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
clang-format -i src/GpioInput.hpp src/GpioInput.cpp
git add src/GpioInput.hpp src/GpioInput.cpp
git commit -m "refactor: GpioInput constructor takes InputConfig"
```

---

### Task 29: GpioOutput takes OutputConfig

**Files:**
- Modify: `src/GpioOutput.hpp`, `src/GpioOutput.cpp`

- [ ] **Step 1: Update header + cpp — constructor takes `const OutputConfig&`**

Unpack `config.pin`, `config.defaultValue`, `config.invert`.

- [ ] **Step 2: Verify + format + commit**

```bash
cd build && cmake .. && make -j$(nproc) && cd ..
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
clang-format -i src/GpioOutput.hpp src/GpioOutput.cpp
git add src/GpioOutput.hpp src/GpioOutput.cpp
git commit -m "refactor: GpioOutput constructor takes OutputConfig"
```

---

### Task 30: PwmOutput takes PwmConfig

Same pattern. Unpack `config.pin`, `config.defaultValue`, `config.invert`.

```bash
git add src/PwmOutput.hpp src/PwmOutput.cpp
git commit -m "refactor: PwmOutput constructor takes PwmConfig"
```

---

### Task 31: SensorInterface takes SensorConfig + Cover takes CoverConfig

**Files:**
- Modify: `src/common/SensorInterface.hpp`, `src/common/SensorInterface.cpp`
- Modify: `src/common/Cover.hpp`, `src/common/Cover.cpp`

SensorInterface is test-built (src/common/). Cover is also test-built. Both must be updated together since Cover creates SensorInterface indirectly. Actually, looking at the code, Cover does NOT create SensorInterface — it creates CoverMovement/CoverStop. The SensorInterface callers are: `config.cpp` (createSensorInterface), `CounterInterface.cpp`. CounterInterface is device-only. So we can update SensorInterface + Cover independently.

- [ ] **Step 1: Update SensorInterface.hpp + .cpp**

Constructor takes `const SensorConfig& config` instead of separate `int interval, int offset, std::vector<std::string> pulse`. Unpack in .cpp.

- [ ] **Step 2: Update Cover.hpp + .cpp**

Constructor takes `const CoverConfig& config` instead of ~12 params. Unpack in .cpp. The `makeUpdateImpl` call needs the individual params — unpack from config.

- [ ] **Step 3: Verify test build (Cover.cpp is test-built)**

```bash
cd build && cmake .. && make -j$(nproc) && ./home_automation_test
```

The existing Cover tests will still pass since the constructor unpacks the same values. But wait — the tests construct `Cover` directly. Let me check.

Looking at `test/CoverTest.cpp`, it likely constructs `Cover` with the current multi-param constructor. If so, changing the constructor breaks the tests. We need to update `CoverTest.cpp` too.

- [ ] **Step 4: Update CoverTest.cpp to use CoverConfig**

Find all `Cover(...)` constructor calls in `test/CoverTest.cpp` and replace with `CoverConfig{...}` construction. Use the existing test helper structure.

- [ ] **Step 5: Verify + format + commit**

```bash
cd build && cmake .. && make -j$(nproc) && ./home_automation_test
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
clang-format -i src/common/SensorInterface.hpp src/common/SensorInterface.cpp src/common/Cover.hpp src/common/Cover.cpp test/CoverTest.cpp
git add src/common/SensorInterface.hpp src/common/SensorInterface.cpp src/common/Cover.hpp src/common/Cover.cpp test/CoverTest.cpp
git commit -m "refactor: SensorInterface takes SensorConfig, Cover takes CoverConfig"
```

---

### Task 32: Remaining device constructors take config structs

Update all remaining device interface constructors. These are all device-only (not test-built), so they only need `arduino-cli` verification.

| Interface | Config struct | Files |
|-----------|--------------|-------|
| AnalogSensor | AnalogConfig | `src/common/AnalogSensor.hpp`, `.cpp` (test-built — verify tests!) |
| EncoderInterface | EncoderConfig | `src/EncoderInterface.hpp`, `.cpp` |
| DhtSensor | DhtConfig | `src/DhtSensor.hpp`, `.cpp` |
| DallasTemperatureSensor | DallasTemperatureConfig | `src/DallasTemperatureSensor.hpp`, `.cpp` |
| HM3301Sensor | Hm3301Config | `src/HM3301Sensor.hpp`, `.cpp` |
| CounterInterface | CounterConfig | `src/CounterInterface.hpp`, `.cpp` |
| EchoDistanceSensor | EchoDistanceConfig | `src/EchoDistanceSensor.hpp`, `.cpp` |
| EchoDistanceReaderInterface | EchoDistanceReaderConfig | `src/EchoDistanceReaderInterface.hpp`, `.cpp` |
| MqttInterface | MqttInterfaceConfig | `src/MqttInterface.hpp`, `.cpp` |
| KeepaliveInterface | KeepaliveConfig | `src/KeepaliveInterface.hpp`, `.cpp` |
| PowerSupplyInterface | PowerSupplyConfig | `src/PowerSupplyInterface.hpp`, `.cpp` |

For each: update header constructor signature, update .cpp constructor to unpack config struct.

Note: AnalogSensor is in `src/common/` (test-built). Check if any test directly constructs AnalogSensor. If so, update those tests too. Looking at the test files, `AnalogSensorTest.cpp` exists — it will need updating.

- [ ] **Step 1: Update each header + .cpp pair**

For each interface, change the constructor to take `const XxxConfig& config` and unpack fields in the implementation.

- [ ] **Step 2: Update AnalogSensorTest.cpp**

Replace direct constructor calls with `AnalogConfig{...}` construction.

- [ ] **Step 3: Verify + format + commit**

```bash
cd build && cmake .. && make -j$(nproc) && ./home_automation_test
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
clang-format -i src/common/AnalogSensor.hpp src/common/AnalogSensor.cpp src/EncoderInterface.hpp src/EncoderInterface.cpp src/DhtSensor.hpp src/DhtSensor.cpp src/DallasTemperatureSensor.hpp src/DallasTemperatureSensor.cpp src/HM3301Sensor.hpp src/HM3301Sensor.cpp src/CounterInterface.hpp src/CounterInterface.cpp src/EchoDistanceSensor.hpp src/EchoDistanceSensor.cpp src/EchoDistanceReaderInterface.hpp src/EchoDistanceReaderInterface.cpp src/MqttInterface.hpp src/MqttInterface.cpp src/KeepaliveInterface.hpp src/KeepaliveInterface.cpp src/PowerSupplyInterface.hpp src/PowerSupplyInterface.cpp test/AnalogSensorTest.cpp
git add -A
git commit -m "refactor: all device interface constructors take config structs"
```

---

## Phase 6: ConfigFactory + config.cpp rewrite

### Task 33: ConfigFactory + config.hpp + config.cpp rewrite

**Files:**
- Modify: `src/config.hpp`
- Modify: `src/config.cpp` (full rewrite of ConfigParser → ConfigFactory)

- [ ] **Step 1: Update config.hpp**

`DeviceConfig` embeds `DeviceConfigCommon`. `GlobalConfig` comes from `common/GlobalConfig.hpp`. Remove `ServerConfig` from config.hpp (now in common). Keep `TopicConfig` as an alias or include from common.

- [ ] **Step 2: Rewrite config.cpp**

Replace the `ConfigParser` class with `ConfigFactory`. The factory:
- Takes `DeviceConfigDescription` (from `ConfigParser`)
- Builds concrete `Interface` objects via `std::visit`
- Handles SPIFFS file reads, Serial init, mqttClient.subscribe
- Re-parses action operation JSON via `operation::Parser`/`Parser2`

`initConfig` flow:
1. `SPIFFS.begin()`
2. Parse files via `JsonParser`
3. `ConfigParser::parseDebugEnabled` → factory.initSerial if true
4. `ConfigParser::parseGlobalConfig` → `globalConfig`
5. `ConfigParser::parseDeviceConfig` → `DeviceConfigDescription`
6. `ConfigFactory::buildDeviceConfig` → `deviceConfig`

- [ ] **Step 3: Verify builds + tests**

```bash
cd build && cmake .. && make -j$(nproc) && ./home_automation_test
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
```

- [ ] **Step 4: Format + commit**

```bash
clang-format -i src/config.hpp src/config.cpp
git add src/config.hpp src/config.cpp
git commit -m "refactor: replace ConfigParser with ConfigFactory in config.cpp"
```

---

## Phase 7: main.cpp adaptation

### Task 34: Update main.cpp to use DeviceConfigCommon fields

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Update field references**

Change `deviceConfig.name` → `deviceConfig.common.name`, `deviceConfig.topics` → `deviceConfig.common.topics`, `deviceConfig.debugTopic` → `deviceConfig.common.debugTopic`, `deviceConfig.debugPort` → `deviceConfig.common.debugPort`, `deviceConfig.interfaces` stays the same.

- [ ] **Step 2: Verify + format + commit**

```bash
cd build && cmake .. && make -j$(nproc) && ./home_automation_test
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
clang-format -i src/main.cpp
git add src/main.cpp
git commit -m "refactor: main.cpp uses DeviceConfigCommon fields"
```

---

## Final Verification

### Task 35: Full build + test + format

- [ ] **Step 1: Run full test suite**

```bash
cd build && cmake .. && make -j$(nproc) && ./home_automation_test
```
Expected: ALL tests pass, including new ConfigParserTest suite.

- [ ] **Step 2: Run device build**

```bash
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
```
Expected: PASS

- [ ] **Step 3: Format all modified files**

```bash
clang-format -i src/common/*.hpp src/common/*.cpp src/*.hpp src/*.cpp test/*.hpp test/*.cpp
```

- [ ] **Step 4: Final commit if any formatting changes**

```bash
git add -A
git diff --cached --quiet || git commit -m "style: format all modified files"
```