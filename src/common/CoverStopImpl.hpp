#ifndef COVER_STOP_IMPL_HPP
#define COVER_STOP_IMPL_HPP

#include <cstdint>
#include <ostream>
#include <string>

#include "CoverStop.hpp"
#include "EspApi.hpp"

class CoverStopImpl : public CoverStop {
public:
    CoverStopImpl(
        EspApi& esp, uint8_t pin, bool latching, bool invertOutput,
        std::ostream& debug, std::string debugPrefix);
    void stop() override;
    void reset() override;
    bool isTriggered() const override;
    bool isLatching() const override;

private:
    EspApi& esp;
    const uint8_t pin;
    const bool latching;
    const bool invertOutput;
    bool triggered = false;
    std::ostream& debug;
    std::string debugPrefix;
};

#endif  // COVER_STOP_IMPL_HPP
