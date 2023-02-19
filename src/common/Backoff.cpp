#include "Backoff.hpp"

Backoff::Backoff(std::ostream& debug, const char* prefix, EspApi& esp, Rtc& rtc,
        unsigned long initialBackoff, unsigned long maximumBackoff)
    : debug(debug)
    , prefix(prefix)
    , esp(esp)
    , rtc(rtc)
    , initialBackoff(initialBackoff)
    , maximumBackoff(maximumBackoff)
    , backoffRtcId(0)
    , currentBackoff(0)
    , lastFailure(0) {
    backoffRtcId = rtc.next();
    currentBackoff = rtc.get(backoffRtcId);
    if (currentBackoff == 0) {
        currentBackoff = initialBackoff;
    }
    debug << prefix << "Initial backoff: " << currentBackoff << std::endl;
}

void Backoff::good() {
    if (lastFailure != 0) {
        debug << prefix << "Reset backoff" << std::endl;
        setBackoff(initialBackoff);
        lastFailure = 0;
    }
}

void Backoff::bad() {
    auto now = esp.millis();
    debug << prefix << "Connection failed" ;
    if (lastFailure == 0) {
        debug << " for the first time. Trying again."
            << std::endl;
        lastFailure = now;
    } else {
        if (now > lastFailure + currentBackoff) {
            debug << ", rebooting." << std::endl;
            setBackoff(std::min(currentBackoff * 2, maximumBackoff));
            esp.restart(true);
            return;
        }
        debug << ", trying again. Rebooting in "
            << static_cast<long>(lastFailure) + currentBackoff - now
            << " ms" << std::endl;
    }
}

void Backoff::setBackoff(unsigned long value) {
    debug << prefix << "New backoff: " << value << std::endl;
    currentBackoff = value;
    rtc.set(backoffRtcId, currentBackoff);
}

