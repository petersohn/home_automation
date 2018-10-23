#ifndef GPIOOUTPUT_HPP
#define GPIOOUTPUT_HPP

#include "common/Interface.hpp"

class GpioOutput : public Interface {
public:
    GpioOutput(int pin, bool defaultValue, bool invert);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    void toggle();
    void setValue();
    void clearBlink();
    bool getOutput();

    int pin;
    unsigned rtcId;
    bool changed = false;
    bool value;
    bool invert;
    unsigned long nextBlink = 0;
    int blinkOn = 0;
    int blinkOff = 0;
};

#endif // GPIOOUTPUT_HPP
