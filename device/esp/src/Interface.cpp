#ifndef INTERFACE_CPP
#define INTERFACE_CPP

#include "Interface.hpp"

Interface::Result GpioInput::answer(String url) {
    if (url.length() != 0) {
        return {false, "Input pin cannot be set"};
    }

    return {true, String(port.getState())};
}

String GpioInput::get() {
    return String(port.getState());
}

Interface::Result GpioOutput::answer(String url) {
    if (url.length() == 0) {
        return {true, String(port.getState())};
    }

    bool value;
    if (url == "1" || url == "on") {
        value = true;
    } else if (url == "0" || url == "off") {
        value = false;
    } else {
        return {false, "Invalid value: " + url};
    }

    port.setState(value);
    return {true, String(value)};
}

String GpioOutput::get() {
    return String(port.getState());
}

#endif // INTERFACE_CPP
