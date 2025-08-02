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
    const auto newPinValue =
        static_cast<PinValue>(digitalRead(downPin) + digitalRead(upPin) * 2);
    if (pinValue == newPinValue) {
        return;
    }

    Serial.println(static_cast<int>(newPinValue));

    if (newPinValue == PinValue::None) {
        state = State::Idle;
    } else if (newPinValue == PinValue::Both) {
        if (state == State::Up) {
            ++value;
        } else if (state == State::Down) {
            --value;
        }
        state = State::Done;
    } else if (state == State::Idle) {
        if (newPinValue == PinValue::Up) {
            state = State::Up;
        } else if (newPinValue == PinValue::Down) {
            state = State::Down;
        }
    } else {
        state = State::Done;
    }

    pinValue = newPinValue;
}

int EspEncoder::read() {
    const int v = value;
    value = 0;
    return v;
}
