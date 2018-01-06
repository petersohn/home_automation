#include "GpioInput.hpp"

#include <Arduino.h>

GpioInput::GpioInput(int pin) {
    pinMode(pin, INPUT);
    bounce.attach(pin);
}

void GpioInput::start() {
    startup = true;
}

void GpioInput::execute(const std::string& /*command*/) {
}

void GpioInput::update(Actions action) {
    if (bounce.update() || startup) {
        action.fire({std::to_string(bounce.read())});
        startup = false;
    }
}

