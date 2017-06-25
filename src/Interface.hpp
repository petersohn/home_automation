#ifndef INTERFACE_HPP
#define INTERFACE_HPP

#include <Arduino.h>

#include "Action.hpp"

class Interface {
public:
    virtual void execute(const String& command) = 0;
    virtual void update(Actions action) = 0;
};

#endif // INTERFACE_HPP
