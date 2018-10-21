#include "AnalogSensor.hpp"
#include "tools/string.hpp"

#include <Arduino.h>

std::vector<std::string> AnalogSensor::measure() {
    return {tools::intToString(analogRead(A0))};
}
