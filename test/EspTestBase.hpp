#ifndef TEST_ESPTESTBASE_HPP
#define TEST_ESPTESTBASE_HPP

#include <ostream>

#include "FakeEspApi.hpp"
#include "FakeRtc.hpp"
#include "FakeWifi.hpp"
#include "TestStream.hpp"

class EspTestBase {
public:
    TestStreambuf debugStreambuf;
    std::ostream debug;
    FakeEspApi esp;
    FakeRtc rtc;
    FakeWifi wifi;

    EspTestBase();
    ~EspTestBase();

    void delayUntil(
        unsigned long time, unsigned long delay, std::function<void()> func);
};

#endif  // TEST_ESPTESTBASE_HPP
