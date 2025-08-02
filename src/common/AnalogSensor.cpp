#include "AnalogSensor.hpp"

#include <algorithm>
#include <cmath>
#include <ostream>

#include "../tools/string.hpp"

AnalogSensor::AnalogSensor(
    EspApi& esp, AnalogInputWithChannel input, double max, int precision,
    unsigned aggregateTime)
    : esp(esp)
    , input(std::move(input))
    , max(max)
    , precision(precision)
    , aggregateTime(aggregateTime) {}

std::optional<std::vector<std::string>> AnalogSensor::measure() {
    auto value = doMeasure();

    if (aggregateTime == 0) {
        return std::vector<std::string>{tools::floatToString(value, precision)};
    }

    auto now = esp.millis();

    if (aggregateBegin == 0) {
        aggregateBegin = now;
        sum = 0.0;
    } else {
        auto avgValue = (value + previousValue) / 2.0;
        sum += avgValue * avgValue * (now - previousTime);
        auto timeDiff = now - aggregateBegin;
        if (timeDiff >= aggregateTime) {
            aggregateBegin = 0;
            return std::vector<std::string>{
                tools::floatToString(std::sqrt(sum / timeDiff), precision)};
        }
    }

    previousValue = value;
    previousTime = now;
    return {};
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
