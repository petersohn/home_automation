#include "AnalogSensor.hpp"

#include <algorithm>
#include <cmath>
#include <ostream>

#include "../tools/string.hpp"

AnalogSensor::AnalogSensor(
    EspApi& esp, AnalogInputWithChannel input, double max, double offset,
    int precision, unsigned aggregateTime)
    : esp(esp)
    , input(std::move(input))
    , max(max)
    , offset(offset)
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
        maxValue = value;
    } else {
        auto avgValue = (value + previousValue) / 2.0;
        sum += avgValue * avgValue * (now - previousTime);
        maxValue = std::max(maxValue, value);
        auto timeDiff = now - aggregateBegin;
        if (timeDiff >= aggregateTime) {
            aggregateBegin = 0;
            return std::vector<std::string>{
                tools::floatToString(std::sqrt(sum / timeDiff), precision),
                tools::floatToString(maxValue, precision),
            };
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
        return value * max / inputMax - offset;
    } else {
        return value;
    }
}
