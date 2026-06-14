#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "FakeEspApi.hpp"
#include "FakeRtc.hpp"
#include "common/CoverState.hpp"
#include "common/CoverStop.hpp"
#include "common/CoverStopImpl.hpp"

namespace {

class CoverStopTest : public ::testing::Test {
protected:
    static constexpr uint8_t stopPin = 5;

    FakeEspApi esp;
    FakeRtc rtc;
    std::ostringstream debug;

    CoverState ctx;

    CoverStopTest()
        : ctx{
              0,            // position
              false,        // stateChanged
              -1,           // activePositionSensor
              -1,           // previouslyActivePositionSensor
              0,            // previousMovementDirection
              -1,           // targetPosition
              0,            // restartCount
              {},           // positionSensors
              false,        // invertInput
              false,        // invertOutput
              false,        // invertPositionSensors
              false,        // latching
              0,            // closedPosition
              0,            // positionId
              this->esp,    // esp
              this->rtc,    // rtc
              this->debug,  // debug
              "test: ",     // debugPrefix
          } {}
};

// ============= Construction =============

TEST_F(CoverStopTest, ConstructionInNonLatchingModeIsNoOp) {
    this->ctx.latching = false;
    CoverStopImpl stopper(this->esp, this->ctx, this->stopPin, false);

    // Pin mode not set
    EXPECT_FALSE(this->esp.getPinMode(this->stopPin).has_value());
    // Not triggered
    EXPECT_FALSE(stopper.isTriggered());
}

TEST_F(CoverStopTest, ConstructionInLatchingModeSetsPinToOutputAndStops) {
    this->ctx.latching = true;
    CoverStopImpl stopper(this->esp, this->ctx, this->stopPin, false);

    // pinMode was set to output
    auto mode = this->esp.getPinMode(this->stopPin);
    ASSERT_TRUE(mode.has_value());
    EXPECT_EQ(*mode, GpioMode::output);
    // stop() activated the pin HIGH (invertOutput=false)
    EXPECT_EQ(this->esp.digitalRead(this->stopPin), 1);
    // Triggered after constructor's stop() call
    EXPECT_TRUE(stopper.isTriggered());
}

TEST_F(CoverStopTest, ConstructionInLatchingModeWithInvertedOutputDrivesLow) {
    this->ctx.latching = true;
    CoverStopImpl stopper(this->esp, this->ctx, this->stopPin, true);

    // stop() drives the pin LOW (invertOutput=true)
    EXPECT_EQ(this->esp.digitalRead(this->stopPin), 0);
    EXPECT_TRUE(stopper.isTriggered());
}

// ============= stop() and reset() — non-latching =============

TEST_F(CoverStopTest, StopInNonLatchingModeIsNoOp) {
    this->ctx.latching = false;
    CoverStopImpl stopper(this->esp, this->ctx, this->stopPin, false);

    stopper.stop();

    // Pin never set (no pinMode called)
    EXPECT_FALSE(this->esp.getPinMode(this->stopPin).has_value());
    // isTriggered stays false (the no-op doesn't claim "stopped")
    EXPECT_FALSE(stopper.isTriggered());
}

TEST_F(CoverStopTest, ResetInNonLatchingModeIsNoOp) {
    this->ctx.latching = false;
    CoverStopImpl stopper(this->esp, this->ctx, this->stopPin, false);

    stopper.reset();

    EXPECT_FALSE(this->esp.getPinMode(this->stopPin).has_value());
    EXPECT_FALSE(stopper.isTriggered());
}

// ============= stop() and reset() — latching =============

TEST_F(CoverStopTest, StopInLatchingModeDrivesPinActive) {
    this->ctx.latching = true;
    CoverStopImpl stopper(this->esp, this->ctx, this->stopPin, false);

    // Reset pin first to a known state
    this->esp.digitalWrite(this->stopPin, 0);
    stopper.reset();
    EXPECT_FALSE(stopper.isTriggered());

    stopper.stop();

    // stop() drives pin HIGH (invertOutput=false)
    EXPECT_EQ(this->esp.digitalRead(this->stopPin), 1);
    EXPECT_TRUE(stopper.isTriggered());
}

TEST_F(CoverStopTest, StopInLatchingModeInvertedDrivesPinLow) {
    this->ctx.latching = true;
    CoverStopImpl stopper(this->esp, this->ctx, this->stopPin, true);

    this->esp.digitalWrite(this->stopPin, 1);
    stopper.reset();
    EXPECT_FALSE(stopper.isTriggered());

    stopper.stop();

    // stop() drives pin LOW (invertOutput=true)
    EXPECT_EQ(this->esp.digitalRead(this->stopPin), 0);
    EXPECT_TRUE(stopper.isTriggered());
}

TEST_F(CoverStopTest, ResetInLatchingModeDrivesPinInactive) {
    this->ctx.latching = true;
    CoverStopImpl stopper(this->esp, this->ctx, this->stopPin, false);

    // After construction the pin is HIGH and stopper is triggered
    EXPECT_TRUE(stopper.isTriggered());
    EXPECT_EQ(this->esp.digitalRead(this->stopPin), 1);

    stopper.reset();

    // reset() drives pin LOW (invertOutput=false)
    EXPECT_EQ(this->esp.digitalRead(this->stopPin), 0);
    EXPECT_FALSE(stopper.isTriggered());
}

TEST_F(CoverStopTest, ResetInLatchingModeInvertedDrivesPinHigh) {
    this->ctx.latching = true;
    CoverStopImpl stopper(this->esp, this->ctx, this->stopPin, true);

    // After construction the pin is LOW and stopper is triggered
    EXPECT_TRUE(stopper.isTriggered());
    EXPECT_EQ(this->esp.digitalRead(this->stopPin), 0);

    stopper.reset();

    // reset() drives pin HIGH (invertOutput=true)
    EXPECT_EQ(this->esp.digitalRead(this->stopPin), 1);
    EXPECT_FALSE(stopper.isTriggered());
}

// ============= State transitions =============

TEST_F(CoverStopTest, IsTriggeredStartsFalseInNonLatchingMode) {
    this->ctx.latching = false;
    CoverStopImpl stopper(this->esp, this->ctx, this->stopPin, false);
    EXPECT_FALSE(stopper.isTriggered());
}

TEST_F(CoverStopTest, MultipleStopCallsKeepTriggered) {
    this->ctx.latching = true;
    CoverStopImpl stopper(this->esp, this->ctx, this->stopPin, false);

    this->esp.digitalWrite(this->stopPin, 0);
    stopper.reset();
    EXPECT_FALSE(stopper.isTriggered());

    stopper.stop();
    EXPECT_TRUE(stopper.isTriggered());

    stopper.stop();
    EXPECT_TRUE(stopper.isTriggered());
}

TEST_F(CoverStopTest, StopResetStopCycle) {
    this->ctx.latching = true;
    CoverStopImpl stopper(this->esp, this->ctx, this->stopPin, false);

    // Construction calls stop() — triggered, pin HIGH
    EXPECT_TRUE(stopper.isTriggered());
    EXPECT_EQ(this->esp.digitalRead(this->stopPin), 1);

    stopper.reset();
    EXPECT_FALSE(stopper.isTriggered());
    EXPECT_EQ(this->esp.digitalRead(this->stopPin), 0);

    stopper.stop();
    EXPECT_TRUE(stopper.isTriggered());
    EXPECT_EQ(this->esp.digitalRead(this->stopPin), 1);
}

}  // namespace
