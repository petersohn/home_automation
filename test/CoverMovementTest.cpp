#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include "FakeEspApi.hpp"
#include "FakeRtc.hpp"
#include "common/CoverMovementContext.hpp"
#include "common/CoverMovementImpl.hpp"
#include "common/CoverStop.hpp"

namespace {

class CoverMovementTest : public ::testing::Test {
protected:
    static constexpr uint8_t inputPin = 1;
    static constexpr uint8_t outputPin = 2;
    static constexpr int upDirection = 1;
    static constexpr int downDirection = -1;
    static constexpr int endPositionUp = 100;
    static constexpr int endPositionDown = 0;

    FakeEspApi esp;
    FakeRtc rtc;
    std::ostringstream debug;

    // Context variables (mutable state owned by test)
    int position = 0;
    bool stateChanged = false;
    int activePositionSensor = -1;
    int previouslyActivePositionSensor = -1;
    int previousMovementDirection = 0;
    int targetPosition = 0;
    unsigned restartCount = 0;
    std::vector<PositionSensor> positionSensors;
    std::string debugPrefix = "test: ";

    CoverMovementContext makeContext() {
        return CoverMovementContext{
            this->position,
            this->stateChanged,
            this->activePositionSensor,
            this->previouslyActivePositionSensor,
            this->previousMovementDirection,
            this->targetPosition,
            this->restartCount,
            this->positionSensors,
            false,  // invertInput
            false,  // invertOutput
            false,  // invertPositionSensors
            0,      // closedPosition
            0,      // positionId
            this->esp,
            this->rtc,
            this->debug,
            this->debugPrefix,
        };
    }

    void advanceMs(unsigned long ms) { this->esp.delay(ms); }

    void expectLogContains(const std::string& expected) {
        auto str = this->debug.str();
        EXPECT_TRUE(str.find(expected) != std::string::npos)
            << "Expected log to contain: " << expected
            << "\nActual log: " << str;
    }
};

// ============= Construction =============

TEST_F(CoverMovementTest, ConstructionWithPositionSensors) {
    this->positionSensors = {
        PositionSensor{0, 10, false},
        PositionSensor{50, 11, false},
        PositionSensor{100, 12, false},
    };
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    // output pin starts LOW
    EXPECT_EQ(this->esp.digitalRead(this->outputPin), 0);
    // For 3 sensors, 2 moveTimes should be allocated
    EXPECT_EQ(this->rtc.next(), 2u);
}

TEST_F(CoverMovementTest, ConstructionWithoutPositionSensors) {
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    EXPECT_EQ(this->esp.digitalRead(this->outputPin), 0);
    // Without position sensors, 1 moveTime should be allocated
    EXPECT_EQ(this->rtc.next(), 1u);
}

// ============= start() and stop() =============

TEST_F(CoverMovementTest, StartActivatesOutput) {
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    this->advanceMs(1);
    movement.start();

    EXPECT_EQ(this->esp.digitalRead(this->outputPin), 1);
}

TEST_F(CoverMovementTest, StartResetsStopper) {
    auto context = this->makeContext();
    // latching stopper
    CoverStop stopper(this->esp, 5, true, false, this->debug, "stop: ");
    // After construction, latching stopper calls stop(): pin 5 = 1
    EXPECT_EQ(this->esp.digitalRead(5), 1);

    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    movement.start();
    // reset() should set pin 5 = 0
    EXPECT_EQ(this->esp.digitalRead(5), 0);
}

TEST_F(CoverMovementTest, StartSetsStartedTime) {
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    this->advanceMs(1);
    EXPECT_FALSE(movement.isStarted());

    movement.start();
    EXPECT_TRUE(movement.isStarted());
}

TEST_F(CoverMovementTest, StopDeactivatesOutput) {
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    this->advanceMs(1);
    movement.start();
    EXPECT_EQ(this->esp.digitalRead(this->outputPin), 1);

    movement.stop();
    EXPECT_EQ(this->esp.digitalRead(this->outputPin), 0);
}

// ============= isMoving() and isStarted() =============

TEST_F(CoverMovementTest, IsMovingReadsInput) {
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    // input pin defaults to 0
    EXPECT_FALSE(movement.isMoving());

    // simulate motor running
    this->esp.digitalWrite(this->inputPin, 1);
    EXPECT_TRUE(movement.isMoving());

    // simulate motor stopped
    this->esp.digitalWrite(this->inputPin, 0);
    EXPECT_FALSE(movement.isMoving());
}

TEST_F(CoverMovementTest, IsStartedReflectsStartedTime) {
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    EXPECT_FALSE(movement.isStarted());

    this->advanceMs(1);
    movement.start();
    EXPECT_TRUE(movement.isStarted());

    movement.stop();
    EXPECT_FALSE(movement.isStarted());
}

TEST_F(CoverMovementTest, ResetStartedSetsStateChanged) {
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    // Before start, startedTime = 0, stop should not set stateChanged
    this->stateChanged = false;
    movement.stop();
    EXPECT_FALSE(this->stateChanged);

    // After start, startedTime != 0, stop should set stateChanged
    this->advanceMs(1);
    movement.start();
    this->stateChanged = false;
    movement.stop();
    EXPECT_TRUE(this->stateChanged);
}

// ============= update() — timeout without moving =============

TEST_F(CoverMovementTest, UpdateStartTimeoutNoPositionSensors) {
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    this->advanceMs(1);
    movement.start();

    // advance well past startTimeout (1000ms)
    this->advanceMs(1500);

    int newPosition = movement.update();

    // Without position sensors, timeout should set position to endPosition
    EXPECT_EQ(newPosition, this->endPositionUp);
    // handleStopped should have been called (stop() -> output LOW)
    EXPECT_EQ(this->esp.digitalRead(this->outputPin), 0);
    EXPECT_FALSE(movement.isStarted());
}

TEST_F(CoverMovementTest, UpdateStartTimeoutWithPositionSensors) {
    this->positionSensors = {
        PositionSensor{0, 10, false},
        PositionSensor{100, 11, false},
    };
    this->position = 50;
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    this->advanceMs(1);
    movement.start();

    this->advanceMs(1500);

    // Clear debug output from construction/start
    this->debug.str("");
    int newPosition = movement.update();

    // With position sensors, timeout should NOT change position (just log)
    EXPECT_EQ(newPosition, 50);
    this->expectLogContains("Did not start.");
    // handleStopped should have been called
    EXPECT_EQ(this->esp.digitalRead(this->outputPin), 0);
    EXPECT_FALSE(movement.isStarted());
}

// ============= update() — moving without position sensors =============

TEST_F(CoverMovementTest, UpdateMovementStartsAfterDebounce) {
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    this->advanceMs(1);
    movement.start();

    // First update: not moving yet
    int pos = movement.update();
    EXPECT_EQ(pos, 0);

    // Motor starts
    this->esp.digitalWrite(this->inputPin, 1);

    // Advance a bit and update: moveStartTime gets set to now
    this->advanceMs(5);
    pos = movement.update();
    EXPECT_EQ(pos, 0);

    // Advance past debounce threshold (need >= 20ms from moveStartTime)
    this->advanceMs(20);
    this->debug.str("");
    pos = movement.update();
    // After debounce, moveStartPosition is set and we get beginPosition +
    // direction moveTime is 0 (unset), so newPosition = beginPosition (0) +
    // direction (1) = 1
    EXPECT_EQ(pos, 1);
    this->expectLogContains("Started moving");
}

TEST_F(CoverMovementTest, UpdateMovementInterpolatesPosition) {
    // Pre-set RTC so moveTime is known
    this->rtc.set(0, 1000);

    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    this->advanceMs(1);
    movement.start();

    // Start motor
    this->esp.digitalWrite(this->inputPin, 1);

    // Advance past debounce and let interpolation run
    this->advanceMs(20);
    movement.update();  // triggers debounce, sets moveStartPosition=0

    // Advance 500ms — with 1000ms total travel time, we expect position ~50
    this->advanceMs(500);
    // Clear debug from debounce logging
    this->debug.str("");
    int pos = movement.update();
    // position = 0 + 100 * 500 / 1000 = 50
    EXPECT_EQ(pos, 50);
}

TEST_F(CoverMovementTest, UpdateMovementEndReached) {
    // Pre-set RTC for moveTime
    this->rtc.set(0, 1000);

    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    this->advanceMs(1);
    movement.start();

    // Motor runs
    this->esp.digitalWrite(this->inputPin, 1);

    // Let it run past debounce and get some interpolation going
    this->advanceMs(20);
    movement.update();
    this->advanceMs(500);
    movement.update();  // interpolated position ~50

    // Motor reaches end stop and stops
    this->esp.digitalWrite(this->inputPin, 0);

    this->debug.str("");
    int pos = movement.update();

    // End reached: position should be endPosition
    EXPECT_EQ(pos, this->endPositionUp);
    EXPECT_EQ(this->esp.digitalRead(this->outputPin), 0);
    EXPECT_FALSE(movement.isStarted());
    this->expectLogContains("End position reached.");
}

// ============= update() — direction down =============

TEST_F(CoverMovementTest, UpdateDownDirection) {
    this->position = 100;
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin,
        this->endPositionDown, this->downDirection, "Down");

    this->advanceMs(1);
    movement.start();

    // First update: not moving yet
    int pos = movement.update();
    EXPECT_EQ(pos, 100);

    // Motor starts
    this->esp.digitalWrite(this->inputPin, 1);

    // Update to set moveStartTime
    this->advanceMs(5);
    pos = movement.update();
    EXPECT_EQ(pos, 100);

    // Advance past debounce
    this->advanceMs(20);
    this->debug.str("");
    pos = movement.update();
    // beginPosition = 100 - 0 = 100, direction = -1
    // Move time is 0, so newPosition = beginPosition + direction = 100 + (-1) =
    // 99
    EXPECT_EQ(pos, 99);

    // Motor stops
    this->esp.digitalWrite(this->inputPin, 0);
    this->debug.str("");
    pos = movement.update();

    EXPECT_EQ(pos, this->endPositionDown);
    EXPECT_EQ(this->esp.digitalRead(this->outputPin), 0);
    EXPECT_FALSE(movement.isStarted());
}

TEST_F(CoverMovementTest, UpdateDownDirectionInterpolates) {
    // Pre-set RTC for moveTime
    this->rtc.set(0, 1000);

    this->position = 100;
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin,
        this->endPositionDown, this->downDirection, "Down");

    this->advanceMs(1);
    movement.start();
    this->esp.digitalWrite(this->inputPin, 1);

    // Advance past debounce
    this->advanceMs(20);
    movement.update();  // triggers debounce, moveStartPosition =
                        // context.position = 100

    // For down direction: beginPosition = 100, endPosition = 0
    // a = (0 - 100) * elapsed = -100 * elapsed
    // d = static_cast<int>(-100 * elapsed / 1000)
    // newPosition = 100 + d
    // After 200ms: d = -20, newPosition = 80
    this->advanceMs(200);
    this->debug.str("");
    int pos = movement.update();
    EXPECT_EQ(pos, 80);
}

// ============= update() — latching mode =============

TEST_F(CoverMovementTest, UpdateLatchingModeResetsStart) {
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 5, true, false, this->debug, "stop: ");

    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    this->advanceMs(1);
    movement.start();
    // startTriggered = true, output pin HIGH

    EXPECT_EQ(this->esp.digitalRead(this->outputPin), 1);

    // Motor starts moving
    this->esp.digitalWrite(this->inputPin, 1);

    this->debug.str("");
    this->advanceMs(10);
    int pos = movement.update();

    // In latching mode with startTriggered and moving:
    // resetStart() is called -> output LOW, startTriggered = false
    EXPECT_EQ(this->esp.digitalRead(this->outputPin), 0);
    this->expectLogContains("Reset start");
    // Position should not change (no debounce yet)
    EXPECT_EQ(pos, 0);

    // Now motor stops
    this->esp.digitalWrite(this->inputPin, 0);
    this->advanceMs(1500);
    this->debug.str("");
    pos = movement.update();

    // In latching mode, handleStopped calls resetStarted (not stop)
    // startedTime should be 0, stateChanged = true
    EXPECT_FALSE(movement.isStarted());
    EXPECT_TRUE(this->stateChanged);
}

TEST_F(CoverMovementTest, StartInLatchingMode) {
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 5, true, false, this->debug, "stop: ");

    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    this->advanceMs(1);
    movement.start();

    // In latching mode, start() calls stopper.reset() then sets output HIGH
    EXPECT_EQ(this->esp.digitalRead(this->outputPin), 1);
    EXPECT_EQ(this->esp.digitalRead(5), 0);
}

// ============= update() — with position sensors =============

TEST_F(CoverMovementTest, UpdateWithPositionSensorReportsBeginPosition) {
    this->positionSensors = {
        PositionSensor{0, 10, false},
        PositionSensor{50, 11, false},
        PositionSensor{100, 12, false},
    };
    this->position = 50;
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    this->advanceMs(1);
    movement.start();

    // Motor starts
    this->esp.digitalWrite(this->inputPin, 1);

    // When a position sensor is active, update should report that position
    this->activePositionSensor = 1;  // position 50
    this->previouslyActivePositionSensor = -1;

    this->advanceMs(10);
    this->debug.str("");
    int pos = movement.update();

    // With active position sensor and paps = -1: calculateMoveTimeIfNeeded(),
    // moveStartTime = 0, position reported from sensor = 50
    EXPECT_EQ(pos, 50);
}

TEST_F(CoverMovementTest, UpdateWithSensorTransitionReportsBeginPlusDirection) {
    this->positionSensors = {
        PositionSensor{0, 10, false},
        PositionSensor{50, 11, false},
        PositionSensor{100, 12, false},
    };
    this->position = 50;
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    this->advanceMs(1);
    movement.start();
    this->esp.digitalWrite(this->inputPin, 1);

    // Motor was just at position sensor 1 (50) and has now left it
    // In the real system, CoverUpdateImpl sets previouslyActivePositionSensor
    this->activePositionSensor = -1;           // no sensor active now
    this->previouslyActivePositionSensor = 1;  // left sensor 1

    this->advanceMs(10);
    this->debug.str("");
    int pos = movement.update();

    // direction > 0 so moveTimeIndex = paps = 1
    // For direction up at sensor boundary 1: begin=50, end=100
    // newPosition = beginPosition + direction = 51
    EXPECT_EQ(pos, 51);
    this->expectLogContains("Just left position sensor 1");
}

// ============= Calculate move time =============

TEST_F(CoverMovementTest, CalculateMoveTimeSavesToRtc) {
    auto context = this->makeContext();
    CoverStop stopper(this->esp, 0, false, false, this->debug, "");
    CoverMovementImpl movement(
        context, stopper, this->inputPin, this->outputPin, this->endPositionUp,
        this->upDirection, "Up");

    this->advanceMs(1);
    movement.start();

    // First update: not moving
    movement.update();

    // Motor starts
    this->esp.digitalWrite(this->inputPin, 1);

    // Update to set moveStartTime
    this->advanceMs(5);
    movement.update();

    // Advance past debounce: moveStartPosition gets set
    this->advanceMs(20);
    movement.update();
    // Now isReallyMoving() = true, moveStartPosition = 0, moveStartTime = 6

    // Let it run for some time
    this->advanceMs(300);

    // Motor stops at end
    this->esp.digitalWrite(this->inputPin, 0);
    this->debug.str("");
    movement.update();

    // calculateMoveTimeIfNeeded should have called rtc.set() for id 0
    // The move time should now be non-zero (~325ms)
    auto moveTime = this->rtc.get(0);
    EXPECT_GT(moveTime, 0u);
    this->expectLogContains("Move time:");
}

}  // namespace
