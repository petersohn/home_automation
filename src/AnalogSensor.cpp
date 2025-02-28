#include "AnalogSensor.hpp"
#include "tools/string.hpp"

#include <Arduino.h>

std::optional<std::vector<std::string>> AnalogSensor::measure() {
    return std::vector<std::string>{tools::intToString(analogRead(A0))};
}
