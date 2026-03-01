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
    this->esp.pinMode(pin, GpioMode::output);
    this->esp.digitalWrite(pin, 0);
}

void PowerSupplyInterface::release(uint8_t pin) {
    this->esp.pinMode(pin, GpioMode::input);
}

void PowerSupplyInterface::execute(const std::string& command) {
    if (command == "on") {
        if (this->targetState != TargetState::On) {
            this->targetState = TargetState::On;
            this->nextCheck = 0;
        }
        return;
    }
    if (command == "off") {
        if (this->targetState != TargetState::Off) {
            this->targetState = TargetState::Off;
            this->nextCheck = 0;
        }
        return;
    }
    if (command == "dontcare") {
        this->targetState = TargetState::Dontcare;
        return;
    }
    if (command == "forceOff") {
        this->targetState = TargetState::Off;
        if (this->esp.digitalRead(this->powerCheckPin) != 0) {
            this->pullDown(this->powerSwitchPin);
            this->powerButtonRelease = this->esp.millis() + this->forceOffTime;
        }
        return;
    }
    if (command == "reset") {
        if (this->targetState != TargetState::Dontcare) {
            this->targetState = TargetState::On;
            this->nextCheck = 0;
        }
        if (this->esp.digitalRead(this->powerCheckPin) != 0) {
            this->pullDown(this->resetSwitchPin);
            this->resetButtonRelease = this->esp.millis() + this->pushTime;
        }
        return;
    }
}

void PowerSupplyInterface::update(Actions /*action*/) {
    auto now = this->esp.millis();
    if (this->powerButtonRelease != 0 && now > this->powerButtonRelease) {
        this->debug << "power button release" << std::endl;
        this->release(this->powerSwitchPin);
        this->powerButtonRelease = 0;
    }
    if (this->resetButtonRelease != 0 && now > this->resetButtonRelease) {
        this->debug << "reset button release" << std::endl;
        this->release(this->resetSwitchPin);
        this->resetButtonRelease = 0;
    }

    if (this->targetState == TargetState::Dontcare || this->nextCheck >= now) {
        return;
    }

    this->debug << "check" << std::endl;
    if (this->esp.digitalRead(this->powerCheckPin) !=
            (this->targetState == TargetState::On ? 1 : 0) &&
        this->powerButtonRelease == 0) {
        this->debug << "power button press" << std::endl;
        this->pullDown(this->powerSwitchPin);
        this->powerButtonRelease = this->esp.millis() + this->pushTime;
    }
    this->nextCheck = now + this->checkTime;
}
