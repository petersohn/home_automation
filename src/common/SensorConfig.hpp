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