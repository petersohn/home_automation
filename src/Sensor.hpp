#ifndef SENSOR_HPP
#define SENSOR_HPP

#include <Arduino.h>

#include <vector>

class Sensor {
public:
    virtual std::vector<String> measure() = 0;
    virtual ~Sensor() {}
};

#endif // SENSOR_HPP
