#ifndef ESPRTC_HPP
#define ESPRTC_HPP

#include "common/rtc.hpp"

class EspRtc : public Rtc {
public:
    EspRtc();
    virtual Data get(unsigned id) override;
    virtual void set(unsigned id, Data value) override;
    virtual unsigned next() override;

private:
    unsigned currentId = 0;
};

#endif  // ESPRTC_HPP
