#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

#include "EspTestBase.hpp"
#include "common/BackoffImpl.hpp"

BOOST_AUTO_TEST_SUITE(BackoffTest)

struct Fixture : public EspTestBase {
    std::unique_ptr<BackoffImpl> backoff;

    Fixture() { reset(); }

    void reset() {
        esp.restarted = false;
        rtc.reset();
        backoff =
            std::make_unique<BackoffImpl>(debug, "test: ", esp, rtc, 10, 50);
    }

    void test(unsigned long delay, bool good, bool shouldRestart = false) {
        esp.delay(delay);
        if (good) {
            backoff->good();
        } else {
            backoff->bad();
        }

        BOOST_REQUIRE_EQUAL(esp.restarted, shouldRestart);

        if (shouldRestart) {
            reset();
        }
    }
};

BOOST_FIXTURE_TEST_CASE(Good, Fixture) {
    BOOST_REQUIRE_NO_THROW(test(5, true));
    BOOST_REQUIRE_NO_THROW(test(10, true));
    BOOST_REQUIRE_NO_THROW(test(10, true));
    BOOST_REQUIRE_NO_THROW(test(15, true));
    BOOST_REQUIRE_NO_THROW(test(30, true));
    BOOST_REQUIRE_NO_THROW(test(50, true));
    BOOST_REQUIRE_NO_THROW(test(50, true));
}

BOOST_FIXTURE_TEST_CASE(Bad, Fixture) {
    BOOST_REQUIRE_NO_THROW(test(5, false));
    BOOST_REQUIRE_NO_THROW(test(5, false));
    BOOST_REQUIRE_NO_THROW(test(6, false, true));

    BOOST_REQUIRE_NO_THROW(test(200, false));
    BOOST_REQUIRE_NO_THROW(test(6, false));
    BOOST_REQUIRE_NO_THROW(test(5, false));
    BOOST_REQUIRE_NO_THROW(test(5, false));
    BOOST_REQUIRE_NO_THROW(test(5, false, true));

    BOOST_REQUIRE_NO_THROW(test(50, false));
    BOOST_REQUIRE_NO_THROW(test(11, false));
    BOOST_REQUIRE_NO_THROW(test(10, false));
    BOOST_REQUIRE_NO_THROW(test(10, false));
    BOOST_REQUIRE_NO_THROW(test(10, false, true));

    BOOST_REQUIRE_NO_THROW(test(150, false));
    BOOST_REQUIRE_NO_THROW(test(11, false));
    BOOST_REQUIRE_NO_THROW(test(10, false));
    BOOST_REQUIRE_NO_THROW(test(10, false));
    BOOST_REQUIRE_NO_THROW(test(10, false));
    BOOST_REQUIRE_NO_THROW(test(10, false, true));

    BOOST_REQUIRE_NO_THROW(test(2, false));
    BOOST_REQUIRE_NO_THROW(test(11, false));
    BOOST_REQUIRE_NO_THROW(test(10, false));
    BOOST_REQUIRE_NO_THROW(test(10, false));
    BOOST_REQUIRE_NO_THROW(test(10, false));
    BOOST_REQUIRE_NO_THROW(test(10, false, true));
}

BOOST_FIXTURE_TEST_CASE(ResetAfterFix, Fixture) {
    BOOST_REQUIRE_NO_THROW(test(5, false));
    BOOST_REQUIRE_NO_THROW(test(5, false));
    BOOST_REQUIRE_NO_THROW(test(6, false, true));

    BOOST_REQUIRE_NO_THROW(test(5, false));
    BOOST_REQUIRE_NO_THROW(test(6, true));

    BOOST_REQUIRE_NO_THROW(test(50, false));
    BOOST_REQUIRE_NO_THROW(test(11, false, true));

    BOOST_REQUIRE_NO_THROW(test(10, false));
    BOOST_REQUIRE_NO_THROW(test(11, false));
    BOOST_REQUIRE_NO_THROW(test(10, false, true));
}

BOOST_AUTO_TEST_SUITE_END()
