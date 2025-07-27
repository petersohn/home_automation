#ifndef ANALOGSENSOR_HPP
#define ANALOGSENSOR_HPP

#include "AnalogInputWithChannel.hpp"
#include "Sensor.hpp"

class AnalogSensor : public Sensor {
public:
    AnalogSensor(AnalogInputWithChannel input, double max, int precision);

    std::optional<std::vector<std::string>> measure() override;

private:
    AnalogInputWithChannel input;
    const double max;
    const int precision;

    double doMeasure();
};

#endif  // ANALOGSENSOR_HPP
