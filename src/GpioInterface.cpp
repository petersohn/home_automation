#include "GpioInterface.hpp"

#include "debug.hpp"

namespace {

String createValue(bool value) {
    return value ? "on" : "off";
}

} // unnamed namespace

void GpioInput::execute(const String& /*command*/) {
}

void GpioInput::update(Actions action) {
    if (bounce.update() || startup) {
        action.fire({createValue(bounce.read())});
        startup = false;
    }
}

bool GpioOutput::getBoolValue(const String& input, bool& output) {
    if (input.equalsIgnoreCase("on") || input.equalsIgnoreCase("true")) {
        output = true;
        return true;
    }
    if (input.equalsIgnoreCase("off") || input.equalsIgnoreCase("false")) {
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
        action.fire({createValue(digitalRead(pin))});
        changed = false;
    }
}
