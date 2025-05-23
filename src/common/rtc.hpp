#ifndef RTC_HPP
#define RTC_HPP

#include <cstdint>

class Rtc {
public:
    using Data = std::uint32_t;

    virtual Data get(unsigned id) = 0;
    virtual void set(unsigned id, Data value) = 0;
    virtual unsigned next() = 0;
};

#endif  // RTC_HPP
