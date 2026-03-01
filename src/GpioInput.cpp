#include "GpioInput.hpp"

#include <Arduino.h>

#include "tools/string.hpp"

extern "C" {
#include "c_types.h"
#include "ets_sys.h"
}

namespace {

enum class StateBits : int { real = 0, saved };

#define IS_BIT(bits, value) \
    (((bits) & (1 << static_cast<int>(StateBits::value))) != 0)
#define SET_BIT(state, bits, value)                            \
    do {                                                       \
        if (state.value) {                                     \
            bits |= (1 << static_cast<int>(StateBits::value)); \
        }                                                      \
    } while (false)

struct State {
    bool real;
    bool saved;
};

State decodeState(int bits) {
    return {IS_BIT(bits, real), IS_BIT(bits, saved)};
}

int encodeState(const State& state) {
    int result = 0;
    SET_BIT(state, result, real);
    SET_BIT(state, result, saved);
    return result;
}

#undef IS_BIT
#undef SET_BIT

}  // namespace

GpioInput::GpioInput(
    std::ostream& debug, uint8_t pin, CycleType cycleType, unsigned interval)
    : debug(debug), pin(pin), cycleType(cycleType), interval(interval) {
    this->pinMode(this->pin, INPUT);
    this->lastChanged = this->millis();
    bool currentState = this->digitalRead(this->pin);
    this->state = encodeState({currentState, currentState});
    this->debug << "starting value=" << currentState << " state=" << this->state
                << std::endl;
    this->attachInterruptArg(this->pin, onChangeStatic, this, CHANGE);
}

void GpioInput::start() {
    this->startup = true;
}

void GpioInput::execute(const std::string& /*command*/) {}

void IRAM_ATTR GpioInput::onChangeStatic(void* arg) {
    static_cast<GpioInput*>(arg)->onChange();
}

void IRAM_ATTR GpioInput::onChange(/* bool newState */) {
    State currentState = decodeState(this->state);
    bool newState = digitalRead(this->pin);
    if (newState == currentState.real) {
        return;
    }
    currentState.real = newState;
    auto now = millis();

    if (now - this->lastChanged > interval) {
        if (currentState.real == currentState.saved) {
            ++this->cycles;
        }
    }

    this->lastChanged = now;
    this->state = encodeState(currentState);
}

void GpioInput::update(Actions action) {
    int currentCycles = 0;
    bool lastState = false;
    State currentState{false, false};
    {
        ETS_GPIO_INTR_DISABLE();
        auto now = millis();
        State decodedState = decodeState(this->state);
        currentCycles = this->cycles;
        lastState = decodedState.saved;
        if (now - this->lastChanged > this->interval) {
            decodedState.saved = decodedState.real;
        }
        this->cycles = 0;
        this->state = encodeState(decodedState);
        ETS_GPIO_INTR_ENABLE();
    }
    this->cycles = 0;
    this->state = encodeState(decodedState);
    ETS_GPIO_INTR_ENABLE();
}

switch (this->cycleType) {
case CycleType::none:
    currentCycles = 0;
    break;
case CycleType::single:
    if (currentCycles > 0) {
        if (currentState.saved == lastState) {
            currentCycles = 1;
        } else {
            currentCycles = 0;
        }
    }
    break;
default:
    break;
}

for (int i = 0; i < currentCycles; ++i) {
    action.fire({tools::intToString(!lastState)});
    action.fire({tools::intToString(lastState)});
}

if ((this->startup && currentCycles == 0) || currentState.saved != lastState) {
    action.fire({tools::intToString(currentState.saved)});
}

this->startup = false;
}
