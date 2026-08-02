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