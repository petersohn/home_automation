#include "PwmOutput.hpp"

#include <Arduino.h>

#include "tools/fromString.hpp"
#include "tools/string.hpp"

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
    auto rtcData = rtc.get(rtcId);
    if (rtcData != 0) {
        currentValue = rtcData - 1;
    }
    analogWrite(pin, currentValue);
}

void PwmOutput::start() {
    changed = true;
}

namespace {
constexpr int maxValue = 255;
}

void PwmOutput::execute(const std::string& command) {
    auto value = tools::fromString<int>(command);
    int realValue = 0;
    if (value.has_value()) {
        if (command[0] == '+' || command[0] == '-') {
            realValue = currentValue + *value;
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
            debug << "Invalid command: " << command << std::endl;
            return;
        }
        realValue = boolValue ? maxValue : 0;
    }

    if (invert) {
        realValue = maxValue - realValue;
    }
    if (realValue != currentValue) {
        currentValue = realValue;
        analogWrite(pin, currentValue);
        rtc.set(rtcId, currentValue);
        changed = true;
    }
}

void PwmOutput::update(Actions action) {
    if (changed) {
        action.fire({tools::intToString(currentValue)});
        changed = false;
    }
}
