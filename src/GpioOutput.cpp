#include <cstdlib>

#include "GpioOutput.hpp"
#include "tools/string.hpp"

namespace {

constexpr int rtcSetMask = 2;
constexpr int rtcValueMask = 1;

}  // unnamed namespace

void GpioOutput::start() {
    changed = true;
}

bool GpioOutput::getOutput() {
    return invert ? !value : value;
}

GpioOutput::GpioOutput(
    std::ostream& debug, EspApi& esp, Rtc& rtc, uint8_t pin, bool defaultValue,
    bool invert)
    : debug(debug)
    , esp(esp)
    , rtc(rtc)
    , pin(pin)
    , rtcId(rtc.next())
    , invert(invert) {
    esp.pinMode(pin, GpioMode::output);
    Rtc::Data rtcData = rtc.get(rtcId);
    debug << "Pin " << static_cast<int>(pin) << ": ";
    if ((rtcData & rtcSetMask) == 0) {
        value = defaultValue;
        debug << "default value ";
    } else {
        value = rtcData & rtcValueMask;
        debug << "from RTC ";
    }
    debug << std::endl;
    setValue();
}

void GpioOutput::execute(const std::string& command) {
    bool newValue = value;
    debug << "Pin " << static_cast<int>(pin)
          << " executing command: " << command << std::endl;

    std::size_t position = 0;
    std::string commandName = tools::nextToken(command, ' ', position);

    if (commandName == "toggle") {
        if (nextBlink != 0) {
            debug << "Cannot toggle while blinking." << std::endl;
        } else {
            toggle();
        }
        return;
    }

    if (commandName == "blink") {
        blinkOn = std::atoi(tools::nextToken(command, ' ', position).c_str());
        blinkOff = std::atoi(tools::nextToken(command, ' ', position).c_str());
        if (blinkOn == 0 || blinkOff == 0) {
            clearBlink();
        } else {
            nextBlink = esp.millis();
        }
        return;
    }

    if (!tools::getBoolValue(
            commandName.c_str(), newValue, commandName.size())) {
        debug << "Invalid command." << std::endl;
        return;
    }

    clearBlink();

    if (value != newValue) {
        toggle();
    }
}

void GpioOutput::update(Actions action) {
    if (nextBlink != 0 && nextBlink <= esp.millis()) {
        toggle();
        nextBlink += value ? blinkOn : blinkOff;
    }

    if (changed) {
        bool localValue = esp.digitalRead(pin);
        action.fire(
            {tools::intToString(invert ? !localValue : localValue),
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
    debug << "Pin " << static_cast<int>(pin) << ": value=" << output
          << std::endl;
    esp.digitalWrite(pin, output);
    Rtc::Data rtcData = rtcSetMask;
    if (value) {
        rtcData |= rtcValueMask;
    }
    rtc.set(rtcId, rtcData);
}
