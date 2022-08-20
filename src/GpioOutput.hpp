#ifndef GPIOOUTPUT_HPP
#define GPIOOUTPUT_HPP

#include "common/Interface.hpp"
#include "common/rtc.hpp"

#include <ostream>

class GpioOutput : public Interface {
public:
    GpioOutput(std::ostream& debug, Rtc& rtc, uint8_t pin, bool defaultValue,
        bool invert);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    std::ostream& debug;
    Rtc& rtc;

    void toggle();
    void setValue();
    void clearBlink();
    bool getOutput();

    uint8_t pin;
    unsigned rtcId;
    bool changed = false;
    bool value;
    bool invert;
    unsigned long nextBlink = 0;
    int blinkOn = 0;
    int blinkOff = 0;
};

#endif // GPIOOUTPUT_HPP
