#include "FakeRtc.hpp"

#include <boost/test/unit_test.hpp>

Rtc::Data FakeRtc::get(unsigned id) {
    auto it = this->data.find(id);
    return it == this->data.end() ? 0 : it->second;
}

void FakeRtc::set(unsigned id, Data value) {
    BOOST_TEST_MESSAGE("RTC set " << id << " = " << value);
    this->data[id] = value;
}

unsigned FakeRtc::next() {
    return this->current++;
}

void FakeRtc::reset() {
    this->current = 0;
}
