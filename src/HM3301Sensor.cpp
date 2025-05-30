#include <Wire.h>

#include "HM3301Sensor.hpp"
#include "tools/string.hpp"

HM3301Sensor::HM3301Sensor(std::ostream& debug, int sda, int scl)
    : debug(debug) {
    Wire.pins(sda, scl);
    initialize();
}

std::optional<std::vector<std::string>> HM3301Sensor::measure() {
    if (!initialize()) {
        return std::vector<std::string>{};
    }

    uint8_t buf[30];
    if (sensor.read_sensor_value(buf, 29)) {
        debug << "HM330X read failed." << std::endl;
        initialized = false;
        return std::vector<std::string>{};
    }

    std::vector<std::string> result;
    for (int i = 2; i < 5; i++) {
        uint16_t value =
            static_cast<uint16_t>(buf[i * 2]) << 8 | buf[i * 2 + 1];
        result.push_back(tools::intToString(value));
    }

    return result;
}

bool HM3301Sensor::initialize() {
    if (initialized) {
        return true;
    }

    if (sensor.init()) {
        debug << "HM330X init failed." << std::endl;
        return false;
    }

    initialized = true;
    return true;
}
