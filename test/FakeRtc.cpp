#include "FakeRtc.hpp"

#include <boost/test/unit_test.hpp>

Rtc::Data FakeRtc::get(unsigned id) {
    auto it = data.find(id);
    return it == data.end() ? 0 : it->second;
}

void FakeRtc::set(unsigned id, Data value) {
    BOOST_TEST_MESSAGE("RTC set " << id << " = " << value);
    data[id] = value;
}

unsigned FakeRtc::next() {
    return current++;
}

void FakeRtc::reset() {
    current = 0;
}
