#ifndef PWMOUTPUT_HPP
#define PWMOUTPUT_HPP

#include <ostream>

#include "common/EspApi.hpp"
#include "common/Interface.hpp"
#include "common/rtc.hpp"

class PwmOutput : public Interface {
public:
    PwmOutput(
        std::ostream& debug, EspApi& esp, Rtc& rtc, uint8_t pin,
        int defaultValue, bool invert);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    std::ostream& debug;
    EspApi& esp;
    Rtc& rtc;
    const uint8_t pin;
    const bool invert;
    const unsigned rtcId;
    int currentValue;
    bool changed = false;
};

#endif  // PWMOUTPUT_HPP
