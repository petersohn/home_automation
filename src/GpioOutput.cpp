#include "GpioOutput.hpp"

#include <cstdlib>

#include "tools/string.hpp"

namespace {

constexpr int rtcSetMask = 2;
constexpr int rtcValueMask = 1;

}  // unnamed namespace

void GpioOutput::start() {
    this->changed = true;
}

bool GpioOutput::getOutput() {
    return this->invert ? !this->value : this->value;
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
    this->esp.pinMode(this->pin, GpioMode::output);
    Rtc::Data rtcData = this->rtc.get(this->rtcId);
    this->debug << "Pin " << static_cast<int>(this->pin) << ": ";
    if ((rtcData & rtcSetMask) == 0) {
        this->value = defaultValue;
        this->debug << "default value ";
    } else {
        this->value = rtcData & rtcValueMask;
        this->debug << "from RTC ";
    }
    this->debug << std::endl;
    setValue();
}

void GpioOutput::execute(const std::string& command) {
    bool newValue = this->value;
    this->debug << "Pin " << static_cast<int>(this->pin)
                << " executing command: " << command << std::endl;

    std::size_t position = 0;
    std::string commandName = tools::nextToken(command, ' ', position);

    if (commandName == "toggle") {
        if (this->nextBlink != 0) {
            this->debug << "Cannot toggle while blinking." << std::endl;
        } else {
            toggle();
        }
        return;
    }

    if (commandName == "blink") {
        this->blinkOn =
            std::atoi(tools::nextToken(command, ' ', position).c_str());
        this->blinkOff =
            std::atoi(tools::nextToken(command, ' ', position).c_str());
        if (this->blinkOn == 0 || this->blinkOff == 0) {
            clearBlink();
        } else {
            this->nextBlink = this->esp.millis();
        }
        return;
    }

    if (!tools::getBoolValue(
            commandName.c_str(), newValue, commandName.size())) {
        this->debug << "Invalid command." << std::endl;
        return;
    }

    clearBlink();

    if (this->value != newValue) {
        toggle();
    }
}

void GpioOutput::update(Actions action) {
    if (this->nextBlink != 0 && this->nextBlink <= this->esp.millis()) {
        toggle();
        this->nextBlink += this->value ? this->blinkOn : this->blinkOff;
    }

    if (this->changed) {
        bool localValue = this->esp.digitalRead(this->pin);
        action.fire(
            {tools::intToString(this->invert ? !localValue : localValue),
             tools::intToString(this->blinkOn),
             tools::intToString(this->blinkOff)});
        this->changed = false;
    }
}

void GpioOutput::toggle() {
    this->value = !this->value;
    setValue();
    this->changed = true;
}

void GpioOutput::clearBlink() {
    if (this->nextBlink != 0) {
        this->nextBlink = 0;
        this->blinkOn = 0;
        this->blinkOff = 0;
        this->changed = true;
    }
}

void GpioOutput::setValue() {
    bool output = getOutput();
    this->debug << "Pin " << static_cast<int>(this->pin) << ": value=" << output
                << std::endl;
    this->esp.digitalWrite(this->pin, output);
    Rtc::Data rtcData = rtcSetMask;
    if (this->value) {
        rtcData |= rtcValueMask;
    }
    this->rtc.set(this->rtcId, rtcData);
}
