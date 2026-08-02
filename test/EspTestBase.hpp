#ifndef TEST_ESPTESTBASE_HPP
#define TEST_ESPTESTBASE_HPP

#include <gtest/gtest.h>

#include <functional>

#include "DebugTestBase.hpp"
#include "FakeEspApi.hpp"
#include "FakeRtc.hpp"
#include "FakeWifi.hpp"

#define ASSERT_NO_FAILURE()                  \
    do {                                     \
        if (::testing::Test::HasFailure()) { \
            FAIL();                          \
        }                                    \
    } while (false)

class LogExpectation;

class EspTestBase : public DebugTestBase {
public:
    FakeEspApi esp;
    FakeRtc rtc;
    FakeWifi wifi;

    EspTestBase();
    ~EspTestBase();

    void delayUntil(
        unsigned long time, unsigned long delay, std::function<void()> func);
    std::shared_ptr<LogExpectation> expectLog(
        std::string log, size_t count = 1);
};

#endif  // TEST_ESPTESTBASE_HPP