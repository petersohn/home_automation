#ifndef DHT_CONFIG_HPP
#define DHT_CONFIG_HPP

#include <cstdint>

#include "SensorConfig.hpp"

namespace dht_type {
constexpr uint8_t AUTO = 0;
constexpr uint8_t DHT11 = 11;
constexpr uint8_t DHT21 = 21;
constexpr uint8_t DHT22 = 22;
}  // namespace dht_type

struct DhtConfig {
    uint8_t pin = 0;
    int type = dht_type::DHT22;
    SensorConfig timing;
};

#endif  // DHT_CONFIG_HPP