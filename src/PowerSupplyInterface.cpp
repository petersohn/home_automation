#include "PowerSupplyInterface.hpp"

PowerSupplyInterface::PowerSupplyInterface(
    std::ostream& debug, EspApi& esp, uint8_t powerSwitchPin,
    uint8_t resetSwitchPin, uint8_t powerCheckPin, unsigned pushTime,
    unsigned forceOffTime, unsigned checkTime, const std::string& initialState)
    : debug(debug)
    , esp(esp)
    , powerSwitchPin(powerSwitchPin)
    , resetSwitchPin(resetSwitchPin)
    , powerCheckPin(powerCheckPin)
    , pushTime(pushTime)
    , forceOffTime(forceOffTime)
    , checkTime(checkTime)
    , targetState(TargetState::Dontcare) {
    if (initialState == "on") {
        targetState = TargetState::On;
    } else if (initialState == "off") {
        targetState = TargetState::Off;
    }

    esp.pinMode(powerSwitchPin, GpioMode::input);
    esp.pinMode(resetSwitchPin, GpioMode::input);
    esp.pinMode(powerCheckPin, GpioMode::input);
}

void PowerSupplyInterface::start() {}

void PowerSupplyInterface::pullDown(uint8_t pin) {
    esp.pinMode(pin, GpioMode::output);
    esp.digitalWrite(pin, 0);
}

void PowerSupplyInterface::release(uint8_t pin) {
    esp.pinMode(pin, GpioMode::input);
}

void PowerSupplyInterface::execute(const std::string& command) {
    if (command == "on") {
        if (targetState != TargetState::On) {
            targetState = TargetState::On;
            nextCheck = 0;
        }
        return;
    }
    if (command == "off") {
        if (targetState != TargetState::Off) {
            targetState = TargetState::Off;
            nextCheck = 0;
        }
        return;
    }
    if (command == "dontcare") {
        targetState = TargetState::Dontcare;
        return;
    }
    if (command == "forceOff") {
        targetState = TargetState::Off;
        if (esp.digitalRead(powerCheckPin) != 0) {
            pullDown(powerSwitchPin);
            powerButtonRelease = esp.millis() + forceOffTime;
        }
        return;
    }
    if (command == "reset") {
        if (targetState != TargetState::Dontcare) {
            targetState = TargetState::On;
            nextCheck = 0;
        }
        if (esp.digitalRead(powerCheckPin) != 0) {
            pullDown(resetSwitchPin);
            resetButtonRelease = esp.millis() + pushTime;
        }
        return;
    }
}

void PowerSupplyInterface::update(Actions /*action*/) {
    auto now = esp.millis();
    if (powerButtonRelease != 0 && now > powerButtonRelease) {
        debug << "power button release" << std::endl;
        release(powerSwitchPin);
        powerButtonRelease = 0;
    }
    if (resetButtonRelease != 0 && now > resetButtonRelease) {
        debug << "reset button release" << std::endl;
        release(resetSwitchPin);
        resetButtonRelease = 0;
    }

    if (targetState == TargetState::Dontcare || nextCheck >= now) {
        return;
    }

    debug << "check" << std::endl;
    if (esp.digitalRead(powerCheckPin) !=
            (targetState == TargetState::On ? 1 : 0) &&
        powerButtonRelease == 0) {
        debug << "power button press" << std::endl;
        pullDown(powerSwitchPin);
        powerButtonRelease = esp.millis() + pushTime;
    }
    nextCheck = now + checkTime;
}
