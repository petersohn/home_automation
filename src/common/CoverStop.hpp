#ifndef COVER_STOP_HPP
#define COVER_STOP_HPP

#include <cstdint>
#include <ostream>
#include <string>

#include "EspApi.hpp"

class CoverStop {
public:
    CoverStop(
        EspApi& esp, uint8_t pin, bool latching, bool invertOutput,
        std::ostream& debug, std::string debugPrefix);
    void stop();
    void reset();
    bool isTriggered() const;
    bool isLatching() const;

private:
    EspApi& esp;
    const uint8_t pin;
    const bool latching;
    const bool invertOutput;
    bool triggered = false;
    std::ostream& debug;
    std::string debugPrefix;
};

#endif  // COVER_STOP_HPP
