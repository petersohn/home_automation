#ifndef ANALOG_CONFIG_HPP
#define ANALOG_CONFIG_HPP

#include <cstdint>
#include <string>

#include "SensorConfig.hpp"

struct AnalogInputWithChannelDescription {
    std::string inputName;
    uint8_t channel = 0;
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