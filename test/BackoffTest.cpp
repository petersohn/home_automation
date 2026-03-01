#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

#include "EspTestBase.hpp"
#include "common/BackoffImpl.hpp"

BOOST_AUTO_TEST_SUITE(BackoffTest)

struct Fixture : public EspTestBase {
    std::unique_ptr<BackoffImpl> backoff;

    Fixture() { this->reset(); }

    void reset() {
        this->esp.restarted = false;
        this->rtc.reset();
        this->backoff = std::make_unique<BackoffImpl>(
            this->debug, "test: ", this->esp, this->rtc, 10, 50);
    }

    void test(unsigned long delay, bool good, bool shouldRestart = false) {
        this->esp.delay(delay);
        if (good) {
            this->backoff->good();
        } else {
            this->backoff->bad();
        }

        BOOST_REQUIRE_EQUAL(this->esp.restarted, shouldRestart);

        if (shouldRestart) {
            this->reset();
        }
    }
};

BOOST_FIXTURE_TEST_CASE(Good, Fixture) {
    BOOST_REQUIRE_NO_THROW(this->test(5, true));
    BOOST_REQUIRE_NO_THROW(this->test(10, true));
    BOOST_REQUIRE_NO_THROW(this->test(10, true));
    BOOST_REQUIRE_NO_THROW(this->test(15, true));
    BOOST_REQUIRE_NO_THROW(this->test(30, true));
    BOOST_REQUIRE_NO_THROW(this->test(50, true));
    BOOST_REQUIRE_NO_THROW(this->test(50, true));
}

BOOST_FIXTURE_TEST_CASE(Bad, Fixture) {
    BOOST_REQUIRE_NO_THROW(this->test(5, false));
    BOOST_REQUIRE_NO_THROW(this->test(5, false));
    BOOST_REQUIRE_NO_THROW(this->test(6, false, true));

    BOOST_REQUIRE_NO_THROW(this->test(200, false));
    BOOST_REQUIRE_NO_THROW(this->test(6, false));
    BOOST_REQUIRE_NO_THROW(this->test(5, false));
    BOOST_REQUIRE_NO_THROW(this->test(5, false));
    BOOST_REQUIRE_NO_THROW(this->test(5, false, true));

    BOOST_REQUIRE_NO_THROW(this->test(50, false));
    BOOST_REQUIRE_NO_THROW(this->test(11, false));
    BOOST_REQUIRE_NO_THROW(this->test(10, false));
    BOOST_REQUIRE_NO_THROW(this->test(10, false));
    BOOST_REQUIRE_NO_THROW(this->test(10, false, true));

    BOOST_REQUIRE_NO_THROW(this->test(150, false));
    BOOST_REQUIRE_NO_THROW(this->test(11, false));
    BOOST_REQUIRE_NO_THROW(this->test(10, false));
    BOOST_REQUIRE_NO_THROW(this->test(10, false));
    BOOST_REQUIRE_NO_THROW(this->test(10, false));
    BOOST_REQUIRE_NO_THROW(this->test(10, false, true));

    BOOST_REQUIRE_NO_THROW(this->test(2, false));
    BOOST_REQUIRE_NO_THROW(this->test(11, false));
    BOOST_REQUIRE_NO_THROW(this->test(10, false));
    BOOST_REQUIRE_NO_THROW(this->test(10, false));
    BOOST_REQUIRE_NO_THROW(this->test(10, false));
    BOOST_REQUIRE_NO_THROW(this->test(10, false, true));
}

BOOST_FIXTURE_TEST_CASE(ResetAfterFix, Fixture) {
    BOOST_REQUIRE_NO_THROW(this->test(5, false));
    BOOST_REQUIRE_NO_THROW(this->test(5, false));
    BOOST_REQUIRE_NO_THROW(this->test(6, false, true));

    BOOST_REQUIRE_NO_THROW(this->test(5, false));
    BOOST_REQUIRE_NO_THROW(this->test(6, true));

    BOOST_REQUIRE_NO_THROW(this->test(50, false));
    BOOST_REQUIRE_NO_THROW(this->test(11, false, true));

    BOOST_REQUIRE_NO_THROW(this->test(10, false));
    BOOST_REQUIRE_NO_THROW(this->test(11, false));
    BOOST_REQUIRE_NO_THROW(this->test(10, false, true));
}

BOOST_AUTO_TEST_SUITE_END()
