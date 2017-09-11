#ifndef GPIOINPUT_HPP
#define GPIOINPUT_HPP

#include "Interface.hpp"

#include <Bounce2.h>

#include <Arduino.h>

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


#endif // GPIOINPUT_HPP
