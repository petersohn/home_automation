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
    auto value = this->doMeasure();

    if (this->aggregateTime == 0) {
        return std::vector<std::string>{
            tools::floatToString(value, this->precision)};
    }

    auto now = this->esp.micros();

    if (this->aggregateBegin == 0 || now < this->previousTime) {
        this->aggregateBegin = now;
        this->sum = 0.0;
        this->maxValue = std::abs(value);
    } else {
        auto avgValue = (value + this->previousValue) / 2.0;
        this->sum += avgValue * avgValue * (now - this->previousTime);
        this->maxValue = std::max(this->maxValue, std::abs(value));
        auto timeDiff = now - this->aggregateBegin;
        if (timeDiff >= this->aggregateTime) {
            this->aggregateBegin = 0;
            return std::vector<std::string>{
                tools::floatToString(
                    std::sqrt(this->sum / timeDiff), this->precision),
                tools::floatToString(this->maxValue, this->precision),
            };
        }
    }

    this->previousValue = value;
    this->previousTime = now;
    return std::nullopt;
}

double AnalogSensor::doMeasure() {
    double value = this->input.read();
    if (this->max != 0.0) {
        const double inputMax = this->input.getMaxValue();
        value = value * this->max / inputMax - this->offset;
    }

    if (std::abs(value) < this->cutoff) {
        return 0.0;
    }

    return value;
}
