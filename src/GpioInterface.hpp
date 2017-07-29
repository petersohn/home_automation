#ifndef GPIOINTERFACE_HPP
#define GPIOINTERFACE_HPP

#include "Interface.hpp"

#include <Bounce2.h>

class GpioInput : public Interface {
public:
    GpioInput(int pin) {
        pinMode(pin, INPUT);
        bounce.attach(pin);
    }

    void start() override;
    void execute(const String& command) override;
    void update(Actions action) override;

private:
    bool startup = false;
    Bounce bounce;
};

class GpioOutput : public Interface {
public:
    GpioOutput(int pin, bool defaultValue);

    void start() override;
    void execute(const String& command) override;
    void update(Actions action) override;

private:
    void toggle();
    void clearBlink();

    int pin;
    bool changed = false;
    bool value;
    unsigned long nextBlink = 0;
    int blinkOn = 0;
    int blinkOff = 0;
};

#endif // GPIOINTERFACE_HPP
