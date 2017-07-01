#include "GpioInterface.hpp"

#include "debug.hpp"
#include "string.hpp"

namespace {

} // unnamed namespace

void GpioInput::execute(const String& /*command*/) {
}

void GpioInput::update(Actions action) {
    if (bounce.update() || startup) {
        action.fire({String(bounce.read())});
        startup = false;
    }
}

void GpioOutput::execute(const String& command) {
    bool newValue = value;
    DEBUG("Pin: ");
    DEBUG(pin);
    DEBUGLN(": executing command: " + command);
    if (!tools::getBoolValue(command, newValue)) {
        DEBUGLN("Invalid command.");
        return;
    }
    if (value != newValue) {
        digitalWrite(pin, newValue);
        value = newValue;
        changed = true;
    }
}

void GpioOutput::update(Actions action) {
    if (changed) {
        action.fire({String(digitalRead(pin))});
        changed = false;
    }
}
