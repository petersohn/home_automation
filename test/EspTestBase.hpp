#ifndef TEST_ESPTESTBASE_HPP
#define TEST_ESPTESTBASE_HPP

#include "FakeEspApi.hpp"
#include "FakeRtc.hpp"

class EspTestBase {
public:
    FakeEspApi esp;
    FakeRtc rtc;
};


#endif // TEST_ESPTESTBASE_HPP
