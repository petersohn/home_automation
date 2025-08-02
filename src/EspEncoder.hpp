#ifndef ESP_ENCODER_HPP
#define ESP_ENCODER_HPP

#include <cstdint>

#include "common/Encoder.hpp"

class EspEncoder : public Encoder {
public:
    EspEncoder(uint8_t downPin, uint8_t upPin);
    virtual int read() override;

private:
    enum class State : uint8_t { Idle, Down, Up, Done };
    enum class PinValue : uint8_t { Both = 0, Up = 1, Down = 2, None = 3 };

    const uint8_t downPin;
    const uint8_t upPin;
    volatile State state = State::Idle;
    volatile PinValue pinValue = PinValue::Both;
    volatile int value = 0;

    static void onChangeStatic(void* arg);
    void onChange();
};

#endif  // ESP_ENCODER_HPP
