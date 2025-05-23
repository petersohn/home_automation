#ifndef COMMON_BACKOFFIMPL_HPP
#define COMMON_BACKOFFIMPL_HPP

#include <functional>
#include <ostream>

#include "Backoff.hpp"
#include "EspApi.hpp"
#include "rtc.hpp"

class BackoffImpl : public Backoff {
public:
    BackoffImpl(
        std::ostream& debug, const char* prefix, EspApi& esp, Rtc& rtc,
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

#endif  // COMMON_BACKOFFIMPL_HPP
