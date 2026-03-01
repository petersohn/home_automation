#include "EspRtc.hpp"

#include <Arduino.h>

#include <cstring>

namespace {

Rtc::Data magic = 1938067743;

constexpr unsigned memorySize = 512;

}  // unnamed namespace

EspRtc::EspRtc() {
    Data value = get(0);
    if (value != magic) {
        Data zero[memorySize / sizeof(Data)];
        std::memset(zero, 0, memorySize);
        ESP.rtcUserMemoryWrite(0, zero, memorySize);
        set(0, magic);
    }
}

Rtc::Data EspRtc::get(unsigned id) {
    Data result;
    ESP.rtcUserMemoryRead(id * sizeof(Data), &result, sizeof(Data));
    return result;
}

void EspRtc::set(unsigned id, Data value) {
    ESP.rtcUserMemoryWrite(id * sizeof(Data), &value, sizeof(Data));
}

unsigned EspRtc::next() {
    return ++this->currentId;
}
