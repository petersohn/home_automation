#ifndef HM3301_CONFIG_HPP
#define HM3301_CONFIG_HPP

#include "SensorConfig.hpp"

struct Hm3301Config {
    int sda = 0;
    int scl = 0;
    SensorConfig timing;
};

#endif  // HM3301_CONFIG_HPP