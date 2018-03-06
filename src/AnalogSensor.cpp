#include "AnalogSensor.hpp"

#include <Arduino.h>

std::vector<std::string> AnalogSensor::measure() {
    return {std::to_string(analogRead(A0))};
}
