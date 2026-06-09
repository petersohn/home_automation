#include "EspTestBase.hpp"
#include "common/BackoffImpl.hpp"

struct BackoffTest : public EspTestBase {
    std::unique_ptr<BackoffImpl> backoff;

    BackoffTest() { this->reset(); }

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

        ASSERT_EQ(this->esp.restarted, shouldRestart);

        if (shouldRestart) {
            this->reset();
        }
    }
};

TEST_F(BackoffTest, Good) {
    ASSERT_NO_FATAL_FAILURE(this->test(5, true));
    ASSERT_NO_FATAL_FAILURE(this->test(10, true));
    ASSERT_NO_FATAL_FAILURE(this->test(10, true));
    ASSERT_NO_FATAL_FAILURE(this->test(15, true));
    ASSERT_NO_FATAL_FAILURE(this->test(30, true));
    ASSERT_NO_FATAL_FAILURE(this->test(50, true));
    ASSERT_NO_FATAL_FAILURE(this->test(50, true));
}

TEST_F(BackoffTest, Bad) {
    ASSERT_NO_FATAL_FAILURE(this->test(5, false));
    ASSERT_NO_FATAL_FAILURE(this->test(5, false));
    ASSERT_NO_FATAL_FAILURE(this->test(6, false, true));

    ASSERT_NO_FATAL_FAILURE(this->test(200, false));
    ASSERT_NO_FATAL_FAILURE(this->test(6, false));
    ASSERT_NO_FATAL_FAILURE(this->test(5, false));
    ASSERT_NO_FATAL_FAILURE(this->test(5, false));
    ASSERT_NO_FATAL_FAILURE(this->test(5, false, true));

    ASSERT_NO_FATAL_FAILURE(this->test(50, false));
    ASSERT_NO_FATAL_FAILURE(this->test(11, false));
    ASSERT_NO_FATAL_FAILURE(this->test(10, false));
    ASSERT_NO_FATAL_FAILURE(this->test(10, false));
    ASSERT_NO_FATAL_FAILURE(this->test(10, false, true));

    ASSERT_NO_FATAL_FAILURE(this->test(150, false));
    ASSERT_NO_FATAL_FAILURE(this->test(11, false));
    ASSERT_NO_FATAL_FAILURE(this->test(10, false));
    ASSERT_NO_FATAL_FAILURE(this->test(10, false));
    ASSERT_NO_FATAL_FAILURE(this->test(10, false));
    ASSERT_NO_FATAL_FAILURE(this->test(10, false, true));

    ASSERT_NO_FATAL_FAILURE(this->test(2, false));
    ASSERT_NO_FATAL_FAILURE(this->test(11, false));
    ASSERT_NO_FATAL_FAILURE(this->test(10, false));
    ASSERT_NO_FATAL_FAILURE(this->test(10, false));
    ASSERT_NO_FATAL_FAILURE(this->test(10, false));
    ASSERT_NO_FATAL_FAILURE(this->test(10, false, true));
}

TEST_F(BackoffTest, ResetAfterFix) {
    ASSERT_NO_FATAL_FAILURE(this->test(5, false));
    ASSERT_NO_FATAL_FAILURE(this->test(5, false));
    ASSERT_NO_FATAL_FAILURE(this->test(6, false, true));

    ASSERT_NO_FATAL_FAILURE(this->test(5, false));
    ASSERT_NO_FATAL_FAILURE(this->test(6, true));

    ASSERT_NO_FATAL_FAILURE(this->test(50, false));
    ASSERT_NO_FATAL_FAILURE(this->test(11, false, true));

    ASSERT_NO_FATAL_FAILURE(this->test(10, false));
    ASSERT_NO_FATAL_FAILURE(this->test(11, false));
    ASSERT_NO_FATAL_FAILURE(this->test(10, false, true));
}
