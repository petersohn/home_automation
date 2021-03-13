#include "PowerSupplyInterface.hpp"
#include "debug.hpp"

#include <Arduino.h>

PowerSupplyInterface::PowerSupplyInterface(int powerSwitchPin,
        int resetSwitchPin, int powerCheckPin, unsigned pushTime,
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

    pinMode(powerSwitchPin, OUTPUT);
    pinMode(resetSwitchPin, OUTPUT);
    pinMode(powerCheckPin, INPUT);
}

void PowerSupplyInterface::start() {
    digitalWrite(powerSwitchPin, 1);
    digitalWrite(resetSwitchPin, 1);
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
            digitalWrite(powerSwitchPin, 0);
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
            digitalWrite(resetSwitchPin, 0);
            resetButtonRelease = millis() + pushTime;
        }
        return;
    }
}

void PowerSupplyInterface::update(Actions /*action*/) {
    auto now = millis();
    if (powerButtonRelease != 0 && now > powerButtonRelease) {
        debugln("power button release");
        digitalWrite(powerSwitchPin, 1);
        powerButtonRelease = 0;
    }
    if (resetButtonRelease != 0 && now > resetButtonRelease) {
        debugln("reset button release");
        digitalWrite(resetSwitchPin, 1);
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
        digitalWrite(powerSwitchPin, 0);
        powerButtonRelease = millis() + pushTime;
    }
    nextCheck = now + checkTime;
}
