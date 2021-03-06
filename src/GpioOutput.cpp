#include "GpioOutput.hpp"

#include "debug.hpp"
#include "rtc.hpp"
#include "tools/string.hpp"

#include <cstdlib>

#include <Arduino.h>

namespace {

constexpr int rtcSetMask = 2;
constexpr int rtcValueMask = 1;

} // unnamed namespace

void GpioOutput::start() {
    changed = true;
}

bool GpioOutput::getOutput() {
    return invert ? !value : value;
}

GpioOutput::GpioOutput(int pin, bool defaultValue, bool invert)
        : pin(pin), rtcId(rtcNext()), invert(invert) {
    pinMode(pin, OUTPUT);
    RtcData rtcData = rtcGet(rtcId);
    debug("Pin ");
    debug(pin);
    debug(": ");
    if ((rtcData & rtcSetMask) == 0) {
        value = defaultValue;
        debug("default value ");
    } else {
        value = rtcData & rtcValueMask;
        debug("from RTC ");
    }
    debugln(value);
    setValue();
}

void GpioOutput::execute(const std::string& command) {
    bool newValue = value;
    debug("Pin: ");
    debug(pin);
    debugln(": executing command: " + command);

    std::size_t position = 0;
    std::string commandName = tools::nextToken(command, ' ', position);

    if (commandName == "toggle") {
        if (nextBlink != 0) {
            debugln("Cannot toggle while blinking.");
        } else {
            toggle();
        }
        return;
    }

    if (commandName =="blink") {
       blinkOn = std::atoi(tools::nextToken(command, ' ', position).c_str());
       blinkOff = std::atoi(tools::nextToken(command, ' ', position).c_str());
        if (blinkOn == 0 || blinkOff == 0) {
            clearBlink();
        } else {
            nextBlink = millis();
        }
        return;
    }

    if (!tools::getBoolValue(commandName, newValue)) {
        debugln("Invalid command.");
        return;
    }

    clearBlink();

    if (value != newValue) {
        toggle();
    }
}

void GpioOutput::update(Actions action) {
    if (nextBlink != 0 && nextBlink <= millis()) {
        toggle();
        nextBlink += value ? blinkOn : blinkOff;
    }

    if (changed) {
        bool localValue = digitalRead(pin);
        action.fire({tools::intToString(invert ? !localValue : localValue),
            tools::intToString(blinkOn), tools::intToString(blinkOff)});
        changed = false;
    }
}

void GpioOutput::toggle() {
    value = !value;
    setValue();
    changed = true;
}

void GpioOutput::clearBlink() {
    nextBlink = 0;
    blinkOn = 0;
    blinkOff = 0;
    changed = true;
}

void GpioOutput::setValue() {
    bool output = getOutput();
    debug("Pin ");
    debug(pin);
    debug(": value=");
    debugln(output);
    digitalWrite(pin, output);
    RtcData rtcData = rtcSetMask;
    if (value) {
        rtcData |= rtcValueMask;
    }
    rtcSet(rtcId, rtcData);
}
