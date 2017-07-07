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

    void execute(const String& command) override;
    void update(Actions action) override;

private:
    bool startup = true;
    Bounce bounce;
};

class GpioOutput : public Interface {
public:
    GpioOutput(int pin) : pin(pin) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, value);
    }

    void execute(const String& command) override;
    void update(Actions action) override;

private:
    void toggle();

    int pin;
    bool changed = true;
    bool value = false;
    unsigned long nextBlink = 0;
    int blinkOn = 0;
    int blinkOff = 0;
};

#endif // GPIOINTERFACE_HPP
