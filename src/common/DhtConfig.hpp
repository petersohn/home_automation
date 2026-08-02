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