#ifndef INTERFACE_HPP
#define INTERFACE_HPP

#include <Arduino.h>

#include "GpioPort.hpp"

class Interface {
public:
    virtual bool set(const String& value) = 0;
    virtual bool get(String& value) = 0;
};
//
// class GpioInput : public Interface {
// public:
//     GpioInput(int pin) : port(pin) {}
//
//     HttpResult answer(DynamicJsonBuffer& buffer, String url) override;
//     String get() override;
//
// private:
//     GpioInputPort port;
// };
//
// class GpioOutput : public Interface {
// public:
//     GpioOutput(int pin) : port(pin) {}
//
//     HttpResult answer(DynamicJsonBuffer& buffer, String url) override;
//     String get() override;
//
// private:
//     GpioOutputPort port;
// };
//
#endif // INTERFACE_HPP
