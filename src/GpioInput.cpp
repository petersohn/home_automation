#include "GpioInput.hpp"

void GpioInput::start() {
    startup = true;
}

void GpioInput::execute(const String& /*command*/) {
}

void GpioInput::update(Actions action) {
    if (bounce.update() || startup) {
        action.fire({String(bounce.read())});
        startup = false;
    }
}

