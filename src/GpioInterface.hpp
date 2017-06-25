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
    int pin;
    bool changed = true;
    bool value = false;
};

#endif // GPIOINTERFACE_HPP
