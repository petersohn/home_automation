#include "AnalogSensor.hpp"

#include <algorithm>
#include <cmath>

#include "../tools/string.hpp"

AnalogSensor::AnalogSensor(
    EspApi& esp, std::ostream& debug, AnalogInputWithChannel input, double max,
    double offset, double cutoff, int precision, unsigned aggregateTime)
    : esp(esp)
    , debug(debug)
    , input(std::move(input))
    , max(max)
    , offset(offset)
    , cutoff(cutoff)
    , precision(precision)
    , aggregateTime(aggregateTime * 1000UL) {}

std::optional<std::vector<std::string>> AnalogSensor::measure() {
    auto value = doMeasure();

    if (aggregateTime == 0) {
        return std::vector<std::string>{tools::floatToString(value, precision)};
    }

    auto now = esp.micros();

    if (aggregateBegin == 0 || now < previousTime) {
        aggregateBegin = now;
        sum = 0.0;
        maxValue = std::abs(value);
    } else {
        auto avgValue = (value + previousValue) / 2.0;
        sum += avgValue * avgValue * (now - previousTime);
        maxValue = std::max(maxValue, std::abs(value));
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
    return std::nullopt;
}

double AnalogSensor::doMeasure() {
    double value = input.read();
    if (max != 0.0) {
        const double inputMax = this->input.getMaxValue();
        value = value * max / inputMax - offset;
    }

    if (std::abs(value) < cutoff) {
        return 0;
    }

    return value;
}
