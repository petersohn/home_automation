#include "GpioInterface.hpp"

#include "debug.hpp"

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

bool GpioOutput::getBoolValue(const String& input, bool& output) {
    if (input == "1" || input.equalsIgnoreCase("on")
            || input.equalsIgnoreCase("true")) {
        output = true;
        return true;
    }
    if (input == "0" || input.equalsIgnoreCase("off")
            || input.equalsIgnoreCase("false")) {
        output = false;
        return true;
    }
    if (input.equalsIgnoreCase("toggle")) {
        output = !value;
        return true;
    }
    return false;
}

void GpioOutput::execute(const String& command) {
    bool newValue = false;
    DEBUG("Pin: ");
    DEBUG(pin);
    DEBUGLN(": executing command: " + command);
    if (!getBoolValue(command, newValue)) {
        DEBUGLN("Invalid command.");
        return;
    }
    DEBUG("Value: ");
    DEBUGLN(newValue);
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
