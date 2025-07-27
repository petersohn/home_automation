#include "AnalogSensor.hpp"

#include <algorithm>
#include <cmath>

#include "../tools/string.hpp"

AnalogSensor::AnalogSensor(
    AnalogInputWithChannel input, double max, int precision)
    : input(std::move(input)), max(max), precision(precision) {}

std::optional<std::vector<std::string>> AnalogSensor::measure() {
    return std::vector<std::string>{
        tools::floatToString(doMeasure(), precision)};
}

double AnalogSensor::doMeasure() {
    double value = input.read();
    if (max != 0.0) {
        double inputMax = this->input.getMaxValue();
        return value * max / inputMax;
    } else {
        return value;
    }
}
