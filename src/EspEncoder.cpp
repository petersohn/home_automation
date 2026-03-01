#include "EspEncoder.hpp"

#include <Arduino.h>

EspEncoder::EspEncoder(uint8_t downPin, uint8_t upPin)
    : downPin(downPin), upPin(upPin) {
    pinMode(downPin, INPUT);
    pinMode(upPin, INPUT);
    attachInterruptArg(downPin, onChangeStatic, this, CHANGE);
    attachInterruptArg(upPin, onChangeStatic, this, CHANGE);

    onChange();
}

void IRAM_ATTR EspEncoder::onChangeStatic(void* arg) {
    static_cast<EspEncoder*>(arg)->onChange();
}

void IRAM_ATTR EspEncoder::onChange() {
    const auto newPinValue = static_cast<PinValue>(
        digitalRead(this->downPin) + digitalRead(this->upPin) * 2);
    if (this->pinValue == newPinValue) {
        return;
    }

    Serial.println(static_cast<int>(newPinValue));

    if (newPinValue == PinValue::None) {
        this->state = State::Idle;
    } else if (newPinValue == PinValue::Both) {
        if (this->state == State::Up) {
            ++this->value;
        } else if (this->state == State::Down) {
            --this->value;
        }
        this->state = State::Done;
    } else if (this->state == State::Idle) {
        if (newPinValue == PinValue::Up) {
            this->state = State::Up;
        } else if (newPinValue == PinValue::Down) {
            this->state = State::Down;
        }
    } else {
        this->state = State::Done;
    }

    this->pinValue = newPinValue;
}

int EspEncoder::read() {
    const int v = this->value;
    this->value = 0;
    return v;
}
