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
    EXPECT_NO_THROW(this->test(5, true));
    EXPECT_NO_THROW(this->test(10, true));
    EXPECT_NO_THROW(this->test(10, true));
    EXPECT_NO_THROW(this->test(15, true));
    EXPECT_NO_THROW(this->test(30, true));
    EXPECT_NO_THROW(this->test(50, true));
    EXPECT_NO_THROW(this->test(50, true));
}

TEST_F(BackoffTest, Bad) {
    EXPECT_NO_THROW(this->test(5, false));
    EXPECT_NO_THROW(this->test(5, false));
    EXPECT_NO_THROW(this->test(6, false, true));

    EXPECT_NO_THROW(this->test(200, false));
    EXPECT_NO_THROW(this->test(6, false));
    EXPECT_NO_THROW(this->test(5, false));
    EXPECT_NO_THROW(this->test(5, false));
    EXPECT_NO_THROW(this->test(5, false, true));

    EXPECT_NO_THROW(this->test(50, false));
    EXPECT_NO_THROW(this->test(11, false));
    EXPECT_NO_THROW(this->test(10, false));
    EXPECT_NO_THROW(this->test(10, false));
    EXPECT_NO_THROW(this->test(10, false, true));

    EXPECT_NO_THROW(this->test(150, false));
    EXPECT_NO_THROW(this->test(11, false));
    EXPECT_NO_THROW(this->test(10, false));
    EXPECT_NO_THROW(this->test(10, false));
    EXPECT_NO_THROW(this->test(10, false));
    EXPECT_NO_THROW(this->test(10, false, true));

    EXPECT_NO_THROW(this->test(2, false));
    EXPECT_NO_THROW(this->test(11, false));
    EXPECT_NO_THROW(this->test(10, false));
    EXPECT_NO_THROW(this->test(10, false));
    EXPECT_NO_THROW(this->test(10, false));
    EXPECT_NO_THROW(this->test(10, false, true));
}

TEST_F(BackoffTest, ResetAfterFix) {
    EXPECT_NO_THROW(this->test(5, false));
    EXPECT_NO_THROW(this->test(5, false));
    EXPECT_NO_THROW(this->test(6, false, true));

    EXPECT_NO_THROW(this->test(5, false));
    EXPECT_NO_THROW(this->test(6, true));

    EXPECT_NO_THROW(this->test(50, false));
    EXPECT_NO_THROW(this->test(11, false, true));

    EXPECT_NO_THROW(this->test(10, false));
    EXPECT_NO_THROW(this->test(11, false));
    EXPECT_NO_THROW(this->test(10, false, true));
}
