#ifndef SENSOR_HPP
#define SENSOR_HPP

#include <string>
#include <vector>

class Sensor {
public:
    virtual std::vector<std::string> measure() = 0;
    virtual ~Sensor() {}
};

#endif // SENSOR_HPP
