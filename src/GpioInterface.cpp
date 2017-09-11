#include "GpioInterface.hpp"

#include "debug.hpp"
#include "rtc.hpp"
#include "string.hpp"

namespace {

constexpr int rtcSetMask = 2;
constexpr int rtcValueMask = 1;

};

void GpioInput::start() {
    startup = true;
}

void GpioInput::execute(const String& /*command*/) {
}

void GpioInput::update(Actions action) {
    if (bounce.update() || startup) {
        action.fire({String(bounce.read())});
        startup = false;
    }
}

void GpioOutput::start() {
    changed = true;
}

GpioOutput::GpioOutput(int pin, bool defaultValue)
        : pin(pin), rtcId(rtcNext()) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, value);
    RtcData rtcData = rtcGet(rtcId);
    value = ((rtcData & rtcSetMask) == 0)
            ? defaultValue : rtcData & rtcValueMask;
    setValue();
}

void GpioOutput::execute(const String& command) {
    bool newValue = value;
    debug("Pin: ");
    debug(pin);
    debugln(": executing command: " + command);

    std::size_t position = 0;
    String commandName = tools::nextToken(command, ' ', position);

    if (commandName.equalsIgnoreCase("toggle")) {
        if (nextBlink != 0) {
            debugln("Cannot toggle while blinking.");
        } else {
            toggle();
        }
        return;
    }

    if (commandName.equalsIgnoreCase("blink")) {
        blinkOn = tools::nextToken(command, ' ', position).toInt();
        blinkOff = tools::nextToken(command, ' ', position).toInt();
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
        action.fire({String(digitalRead(pin)),
                String(blinkOn), String(blinkOff)});
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
    digitalWrite(pin, value);
    RtcData rtcData = rtcSetMask;
    if (value) {
        rtcData |= rtcValueMask;
    }
    rtcSet(rtcId, rtcData);
}
