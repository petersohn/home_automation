#ifndef INTERFACE_HPP
#define INTERFACE_HPP

#include <Arduino.h>
#include <ArduinoJson.h>

#include "result.hpp"
#include "GpioPort.hpp"

class Interface {
public:
    virtual HttpResult answer(DynamicJsonBuffer& buffer, String url) = 0;
    virtual String get() = 0;
};

class GpioInput : public Interface {
public:
    GpioInput(int pin) : port(pin) {}

    HttpResult answer(DynamicJsonBuffer& buffer, String url) override;
    String get() override;

private:
    GpioInputPort port;
};

class GpioOutput : public Interface {
public:
    GpioOutput(int pin) : port(pin) {}

    HttpResult answer(DynamicJsonBuffer& buffer, String url) override;
    String get() override;

private:
    GpioOutputPort port;
};

#endif // INTERFACE_HPP
