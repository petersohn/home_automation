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