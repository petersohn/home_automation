#include "PowerSupplyInterface.hpp"
#include "debug.hpp"

#include <Arduino.h>

PowerSupplyInterface::PowerSupplyInterface(uint8_t powerSwitchPin,
        uint8_t resetSwitchPin, uint8_t powerCheckPin, unsigned pushTime,
        unsigned forceOffTime, unsigned checkTime, const std::string& initialState)
    : powerSwitchPin(powerSwitchPin)
    , resetSwitchPin(resetSwitchPin)
    , powerCheckPin(powerCheckPin)
    , pushTime(pushTime)
    , forceOffTime(forceOffTime)
    , checkTime(checkTime)
    , targetState(TargetState::Dontcare)
{
    if (initialState == "on") {
        targetState = TargetState::On;
    } else if (initialState == "off") {
        targetState = TargetState::Off;
    }

    pinMode(powerSwitchPin, INPUT);
    pinMode(resetSwitchPin, INPUT);
    pinMode(powerCheckPin, INPUT);
}

void PowerSupplyInterface::start() {
}

void PowerSupplyInterface::pullDown(uint8_t pin)
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, 0);
}

void PowerSupplyInterface::release(uint8_t pin)
{
    pinMode(pin, INPUT);
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
        if (digitalRead(powerCheckPin) != 0) {
            pullDown(powerSwitchPin);
            powerButtonRelease = millis() + forceOffTime;
        }
        return;
    }
    if (command == "reset") {
        if (targetState != TargetState::Dontcare) {
            targetState = TargetState::On;
            nextCheck = 0;
        }
        if (digitalRead(powerCheckPin) != 0) {
            pullDown(resetSwitchPin);
            resetButtonRelease = millis() + pushTime;
        }
        return;
    }
}

void PowerSupplyInterface::update(Actions /*action*/) {
    auto now = millis();
    if (powerButtonRelease != 0 && now > powerButtonRelease) {
        debugln("power button release");
        release(powerSwitchPin);
        powerButtonRelease = 0;
    }
    if (resetButtonRelease != 0 && now > resetButtonRelease) {
        debugln("reset button release");
        release(resetSwitchPin);
        resetButtonRelease = 0;
    }

    if (targetState == TargetState::Dontcare || nextCheck >= now) {
        return;
    }

    debugln("check");
    if (digitalRead(powerCheckPin) !=
            (targetState == TargetState::On ? 1 : 0) &&
            powerButtonRelease == 0) {
        debugln("power button press");
        pullDown(powerSwitchPin);
        powerButtonRelease = millis() + pushTime;
    }
    nextCheck = now + checkTime;
}
