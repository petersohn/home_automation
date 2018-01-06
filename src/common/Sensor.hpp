#ifndef COMMON_SENSOR_HPP
#define COMMON_SENSOR_HPP

#include <string>
#include <vector>

class Sensor {
public:
    virtual std::vector<std::string> measure() = 0;
    virtual ~Sensor() {}
};

#endif // COMMON_SENSOR_HPP
