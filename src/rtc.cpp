#include "rtc.hpp"

#include <Arduino.h>

#include <cstring>

namespace {

RtcData magic = 1938067743;

unsigned currentId = 0;

constexpr unsigned memorySize = 512;

} // unnamed namespace

void rtcInit() {
    RtcData value = rtcGet(0);
    if (value != magic) {
        RtcData zero[memorySize / sizeof(RtcData)];
        std::memset(zero, 0, memorySize);
        ESP.rtcUserMemoryWrite(0, zero, memorySize);
        rtcSet(0, magic);
    }
}

RtcData rtcGet(unsigned id) {
    RtcData result;
    ESP.rtcUserMemoryRead(id * sizeof(RtcData), &result, sizeof(RtcData));
    return result;
}

void rtcSet(unsigned id, RtcData value) {
    ESP.rtcUserMemoryWrite(id * sizeof(RtcData), &value, sizeof(RtcData));
}

unsigned rtcNext() {
    return ++currentId;
}

