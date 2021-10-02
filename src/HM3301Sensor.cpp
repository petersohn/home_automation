#include "HM3301Sensor.hpp"

#include "debug.hpp"
#include "tools/string.hpp"

#include <Wire.h>

HM3301Sensor::HM3301Sensor(int sda, int scl) {
    Wire.pins(sda, scl);
    initialize();
}

std::vector<std::string> HM3301Sensor::measure() {
    if (!initialize()) {
        return {};
    }

    uint8_t buf[30];
    if (sensor.read_sensor_value(buf, 29)) {
        debugln("HM330X read failed.");
        initialized = false;
        return {};
    }

    std::vector<std::string> result;
    for (int i = 2; i < 5; i++) {
        uint16_t value = static_cast<uint16_t>(buf[i * 2]) << 8 | buf[i * 2 + 1];
        result.push_back(tools::intToString(value));
    }

    return result;
}

bool HM3301Sensor::initialize() {
    if (initialized) {
        return true;
    }

    if (sensor.init()) {
        debugln("HM330X init failed.");
        return false;
    }

    initialized = true;
    return true;
}
