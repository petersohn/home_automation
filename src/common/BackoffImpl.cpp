#include "BackoffImpl.hpp"

BackoffImpl::BackoffImpl(
    std::ostream& debug, const char* prefix, EspApi& esp, Rtc& rtc,
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
    this->backoffRtcId = this->rtc.next();
    this->currentBackoff = this->rtc.get(this->backoffRtcId);
    if (this->currentBackoff == 0) {
        this->currentBackoff = this->initialBackoff;
    }
    this->debug << this->prefix << "Initial backoff: " << this->currentBackoff
                << std::endl;
}

void BackoffImpl::good() {
    if (this->lastFailure != 0) {
        this->debug << this->prefix << "Reset backoff" << std::endl;
        this->setBackoff(this->initialBackoff);
        this->lastFailure = 0;
    }
}

void BackoffImpl::bad() {
    auto now = this->esp.millis();
    this->debug << this->prefix << "Connection failed";
    if (this->lastFailure == 0) {
        this->debug << " for the first time. Trying again." << std::endl;
        this->lastFailure = now;
    } else {
        if (now > this->lastFailure + this->currentBackoff) {
            this->debug << ", rebooting." << std::endl;
            this->setBackoff(
                std::min(this->currentBackoff * 2, this->maximumBackoff));
            this->esp.restart(true);
            return;
        }
        this->debug << ", trying again. Rebooting in "
                    << static_cast<long>(this->lastFailure) +
                           this->currentBackoff - now
                    << " ms" << std::endl;
    }
}

void BackoffImpl::setBackoff(unsigned long value) {
    this->debug << this->prefix << "New backoff: " << value << std::endl;
    this->currentBackoff = value;
    this->rtc.set(this->backoffRtcId, this->currentBackoff);
}
