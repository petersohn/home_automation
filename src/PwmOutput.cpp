#include "PwmOutput.hpp"

#include <Arduino.h>

#include "tools/fromString.hpp"
#include "tools/string.hpp"

namespace {
constexpr int maxValue = 255;
}

PwmOutput::PwmOutput(
    std::ostream& debug, EspApi& esp, Rtc& rtc, uint8_t pin, int defaultValue,
    bool invert)
    : debug(debug)
    , esp(esp)
    , rtc(rtc)
    , pin(pin)
    , invert(invert)
    , rtcId(rtc.next())
    , currentValue(defaultValue) {
    auto rtcData = this->rtc.get(this->rtcId);
    if (rtcData != 0) {
        this->currentValue = rtcData - 1;
    }
    this->esp.pinMode(this->pin, GpioMode::output);
    set();
}

void PwmOutput::set() {
    analogWrite(
        this->pin,
        this->invert ? maxValue - this->currentValue : this->currentValue);
}

void PwmOutput::start() {
    this->changed = true;
}

void PwmOutput::execute(const std::string& command) {
    auto value = tools::fromString<int>(command);
    int realValue = 0;
    if (value.has_value()) {
        if (command[0] == '+' || command[0] == '-') {
            realValue = this->currentValue + *value;
        } else {
            realValue = *value;
        }
        if (realValue < 0) {
            realValue = 0;
        }
        if (realValue > maxValue) {
            realValue = maxValue;
        }
    } else {
        bool boolValue = false;
        if (!tools::getBoolValue(command.c_str(), boolValue)) {
            this->debug << "Invalid command: " << command << std::endl;
            return;
        }
        realValue = boolValue ? maxValue : 0;
    }

    if (realValue != this->currentValue) {
        this->currentValue = realValue;
        set();
        this->rtc.set(this->rtcId, this->currentValue);
        this->changed = true;
    }
}

void PwmOutput::update(Actions action) {
    if (this->changed) {
        action.fire({tools::intToString(this->currentValue)});
        this->changed = false;
    }
}
