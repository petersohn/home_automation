#ifndef COMMON_SENSOR_HPP
#define COMMON_SENSOR_HPP

#include <string>
#include <vector>
#include <optional>

class Sensor {
public:
    virtual std::optional<std::vector<std::string>> measure() = 0;
    virtual ~Sensor() {}
};

#endif // COMMON_SENSOR_HPP
