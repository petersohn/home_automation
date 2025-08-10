#ifndef ANALOGSENSOR_HPP
#define ANALOGSENSOR_HPP

#include <ostream>

#include "AnalogInputWithChannel.hpp"
#include "EspApi.hpp"
#include "Sensor.hpp"

class AnalogSensor : public Sensor {
public:
    AnalogSensor(
        EspApi& esp, AnalogInputWithChannel input, double max, double offset,
        int precision, unsigned aggregateTime);

    std::optional<std::vector<std::string>> measure() override;

private:
    EspApi& esp;
    AnalogInputWithChannel input;

    const double max;
    const double offset;
    const int precision;
    const unsigned aggregateTime;

    unsigned long aggregateBegin = 0;
    double sum = 0.0;
    double maxValue = 0.0;
    double previousValue = 0.0;
    unsigned long previousTime = 0;

    double doMeasure();
};

#endif  // ANALOGSENSOR_HPP
