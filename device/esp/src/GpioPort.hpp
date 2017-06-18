#ifndef GPIOPORT_HPP
#define GPIOPORT_HPP

#include <Arduino.h>

class GpioInputPort {
public:
    GpioInputPort(int pin) : pin(pin) {
        pinMode(pin, INPUT);
    }

    bool getState() {
        constexpr int pressThreshold = 100;
        unsigned long now = millis();
        if (now - lastSeen > pressThreshold) {
            lastSeen = now;
            value = digitalRead(pin);
        }
        return value;
    }
protected:
    const int pin;
    unsigned long lastSeen = millis();
    bool value = digitalRead(pin);
};

class GpioOutputPort {
public:
    GpioOutputPort(int pin) : pin(pin) {
        pinMode(pin, OUTPUT);
    }

    bool getState() {
        return digitalRead(pin);
    }

    void setState(bool state) {
        digitalWrite(pin, state);
    }

private:
    int pin;
};

#endif // GPIOPORT_HPP
