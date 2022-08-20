#include "GpioInput.hpp"

#include "tools/string.hpp"


namespace {

enum class StateBits : int {
    real = 0, saved
};

#define IS_BIT(bits, value) \
    (((bits) & (1 << static_cast<int>(StateBits::value))) != 0)
#define SET_BIT(state, bits, value) do { \
    if (state.value) { \
        bits |= (1 << static_cast<int>(StateBits::value)); \
    } \
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

}

GpioInput::GpioInput(std::ostream& debug, EspApi& esp, uint8_t pin,
        CycleType cycleType,
    unsigned interval)
    : debug(debug), esp(esp), pin(pin), cycleType(cycleType), interval(interval) {
    esp.pinMode(pin, GpioMode::input);
    lastChanged = esp.millis();
    bool currentState = esp.digitalRead(pin);
    state = encodeState({currentState, currentState});
    debug << "starting value=" << currentState << " state=" << state << std::endl;
    esp.attachInterrupt(pin, [this]() { onChange(); }, InterruptMode::change);
}

void GpioInput::start() {
    startup = true;
}

void GpioInput::execute(const std::string& /*command*/) {
}

void GpioInput::onChange() {
    State currentState = decodeState(state);
    bool newState = esp.digitalRead(pin);
    if (newState == currentState.real) {
        return;
    }
    currentState.real = newState;
    auto now = esp.millis();

    if (now - lastChanged > interval) {
        if (currentState.real == currentState.saved) {
            ++cycles;
        }
    }

    lastChanged = now;
    state = encodeState(currentState);
}

void GpioInput::update(Actions action) {
    int currentCycles = 0;
    bool lastState = false;
    State currentState{false, false};
    {
        auto guard = esp.disableInterrupt();
        auto now = esp.millis();
        currentState = decodeState(state);
        currentCycles = cycles;
        lastState = currentState.saved;
        if (now - lastChanged > interval) {
            currentState.saved = currentState.real;
        }
        cycles = 0;
        state = encodeState(currentState);
    }

    switch (cycleType) {
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

    if ((startup && currentCycles == 0) || currentState.saved != lastState) {
        action.fire({tools::intToString(currentState.saved)});
    }

    startup = false;
}

