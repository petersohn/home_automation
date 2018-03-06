#ifndef ANALOGSENSOR_HPP
#define ANALOGSENSOR_HPP

#include "common/Sensor.hpp"

class AnalogSensor : public Sensor {
public:
    AnalogSensor() {}

    std::vector<std::string> measure() override;
};


#endif // ANALOGSENSOR_HPP
