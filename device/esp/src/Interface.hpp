#ifndef INTERFACE_HPP
#define INTERFACE_HPP

#include <Arduino.h>

#include "GpioPort.hpp"

class Interface {
public:
    struct Result {
        bool success;
        String value;
    };

    virtual Result answer(String url) = 0;
    virtual String get() = 0;
};

class GpioInput : public Interface {
public:
    GpioInput(int pin) : port(pin) {}

    Result answer(String url) override;
    String get() override;

private:
    GpioInputPort port;
};

class GpioOutput : public Interface {
public:
    GpioOutput(int pin) : port(pin) {}

    Result answer(String url) override;
    String get() override;

private:
    GpioOutputPort port;
};

#endif // INTERFACE_HPP
