#ifndef COMMON_BACKOFF_HPP
#define COMMON_BACKOFF_HPP

#include "rtc.hpp"
#include "EspApi.hpp"

#include <functional>
#include <ostream>

class Backoff {
public:
    Backoff(std::ostream& debug, const char* prefix, EspApi& esp, Rtc& rtc,
            unsigned long initialBackoff, unsigned long maximumBackoff);

    void good();
    void bad();

private:
    std::ostream& debug;
    const char* prefix;
    EspApi& esp;
    Rtc& rtc;
    unsigned long initialBackoff;
    unsigned long maximumBackoff;

    unsigned backoffRtcId;
    unsigned long currentBackoff;
    unsigned long lastFailure;

    void setBackoff(unsigned long value);
};

#endif // COMMON_BACKOFF_HPP
