#ifndef RTC_HPP
#define RTC_HPP

#include <cstdint>

using RtcData = std::uint32_t;

void rtcInit();
RtcData rtcGet(unsigned id);
void rtcSet(unsigned id, RtcData value);
unsigned rtcNext();

#endif // RTC_HPP
