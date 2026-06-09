#include "FakeRtc.hpp"

#include <iostream>

Rtc::Data FakeRtc::get(unsigned id) {
    auto it = this->data.find(id);
    return it == this->data.end() ? 0 : it->second;
}

void FakeRtc::set(unsigned id, Data value) {
    std::cout << "RTC set " << id << " = " << value << std::endl;
    this->data[id] = value;
}

unsigned FakeRtc::next() {
    return this->current++;
}

void FakeRtc::reset() {
    this->current = 0;
}
