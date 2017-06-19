#ifndef INTERFACE_CPP
#define INTERFACE_CPP

#include "Interface.hpp"

namespace {

String getBinaryState(bool value) {
    return value ? "on" : "off";
}

} // unnamed namespace

Interface::Result GpioInput::answer(String url) {
    if (url.length() != 0) {
        return {false, "Input pin cannot be set"};
    }

    return {true, String(port.getState())};
}

String GpioInput::get() {
    return getBinaryState(port.getState());
}

Interface::Result GpioOutput::answer(String url) {
    if (url.length() == 0) {
        return {true, String(port.getState())};
    }

    bool value;
    if (url == "1" || url.equalsIgnoreCase("on")) {
        value = true;
    } else if (url == "0" || url.equalsIgnoreCase("off")) {
        value = false;
    } else {
        return {false, "Invalid value: " + url};
    }

    port.setState(value);
    return {true, String(value)};
}

String GpioOutput::get() {
    return getBinaryState(port.getState());
}

#endif // INTERFACE_CPP
