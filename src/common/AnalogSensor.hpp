#ifndef ANALOGSENSOR_HPP
#define ANALOGSENSOR_HPP

#include "AnalogInputWithChannel.hpp"
#include "Sensor.hpp"

class AnalogSensor : public Sensor {
public:
    AnalogSensor(AnalogInputWithChannel input) : input(std::move(input)) {}

    std::optional<std::vector<std::string>> measure() override;

private:
    AnalogInputWithChannel input;
};

#endif  // ANALOGSENSOR_HPP
