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