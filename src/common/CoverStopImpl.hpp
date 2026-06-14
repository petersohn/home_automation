#ifndef COVER_STOP_IMPL_HPP
#define COVER_STOP_IMPL_HPP

#include <cstdint>
#include <ostream>
#include <string>

#include "CoverState.hpp"
#include "CoverStop.hpp"
#include "EspApi.hpp"

class CoverStopImpl : public CoverStop {
public:
    CoverStopImpl(
        EspApi& esp, CoverState& state, uint8_t pin, bool invertOutput);
    void stop() override;
    void reset() override;
    bool isTriggered() const override;

private:
    EspApi& esp;
    CoverState& state;
    const uint8_t pin;
    const bool invertOutput;
    bool triggered = false;
};

#endif  // COVER_STOP_IMPL_HPP
