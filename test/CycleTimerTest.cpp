#include <gtest/gtest.h>

#include "EspTestBase.hpp"
#include "common/CycleTimer.hpp"

class CycleTimerTest : public EspTestBase {
public:
    CycleTimer timer{this->esp};

    void advance(unsigned long ms) {
        this->esp.delay(ms);
        this->timer.tick();
    }
};

TEST_F(CycleTimerTest, NoTicksNoData) {
    EXPECT_EQ(timer.getCycles(), 0);
    EXPECT_EQ(timer.getMaxCycleTime(), 0);
    EXPECT_EQ(timer.getAvgCycleTime(), 0);
}

TEST_F(CycleTimerTest, FirstTickSetsBaseline) {
    this->esp.delay(1000);
    timer.tick();
    EXPECT_EQ(timer.getCycles(), 0);
    EXPECT_EQ(timer.getMaxCycleTime(), 0);
    EXPECT_EQ(timer.getAvgCycleTime(), 0);
}

TEST_F(CycleTimerTest, CountsTimeBetweenTicks) {
    timer.tick();
    advance(10);
    advance(40);

    EXPECT_EQ(timer.getCycles(), 2);
    EXPECT_EQ(timer.getMaxCycleTime(), 40);
    EXPECT_FLOAT_EQ(timer.getAvgCycleTime(), 25);
}

TEST_F(CycleTimerTest, LongGapIsCountedWhenTickingContinues) {
    // A long gap between ticks means the loop was blocked: it must be
    // visible in maxCycleTime.
    timer.tick();
    this->esp.delay(9000);
    timer.tick();

    EXPECT_EQ(timer.getCycles(), 1);
    EXPECT_EQ(timer.getMaxCycleTime(), 9000);
}

TEST_F(CycleTimerTest, ResetClearsData) {
    timer.tick();
    advance(20);
    EXPECT_EQ(timer.getCycles(), 1);
    EXPECT_EQ(timer.getMaxCycleTime(), 20);

    timer.reset();
    EXPECT_EQ(timer.getCycles(), 0);
    EXPECT_EQ(timer.getMaxCycleTime(), 0);

    advance(5);  // baseline tick
    EXPECT_EQ(timer.getCycles(), 0);
    advance(5);
    EXPECT_EQ(timer.getCycles(), 1);
    EXPECT_EQ(timer.getMaxCycleTime(), 5);
}

TEST_F(CycleTimerTest, ResetAfterLongGap) {
    // A reset marks the end of a silent period (e.g. wifi connect): the gap
    // before the first tick afterwards must not count as cycle time.
    timer.tick();
    advance(20);
    timer.reset();

    this->esp.delay(9000);  // long silence
    timer.tick();
    advance(10);

    EXPECT_EQ(timer.getCycles(), 1);
    EXPECT_EQ(timer.getMaxCycleTime(), 10);
    EXPECT_FLOAT_EQ(timer.getAvgCycleTime(), 10);
}
