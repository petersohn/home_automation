#include <Arduino.h>

#include "AnalogSensor.hpp"
#include "tools/string.hpp"

std::optional<std::vector<std::string>> AnalogSensor::measure() {
    return std::vector<std::string>{tools::intToString(analogRead(A0))};
}
