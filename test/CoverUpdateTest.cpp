#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include "EspTestBase.hpp"
#include "common/Actions.hpp"
#include "common/CoverMovement.hpp"
#include "common/CoverMovementContext.hpp"
#include "common/CoverStop.hpp"
#include "common/CoverUpdate.hpp"
#include "common/InterfaceConfig.hpp"
#include "common/PositionSensor.hpp"

// Hand-rolled test double for CoverMovement.
// Tracks start/stop calls and allows configuring return values.
class FakeCoverMovement : public CoverMovement {
public:
    void start() override { ++this->startCount_; }
    void stop() override { ++this->stopCount_; }
    bool isMoving() const override { return this->moving_; }
    bool isStarted() const override { return this->started_; }
    int update() override { return this->updateReturn_; }

    int startCount() const { return this->startCount_; }
    int stopCount() const { return this->stopCount_; }

    void setMoving(bool v) { this->moving_ = v; }
    void setStarted(bool v) { this->started_ = v; }
    void setUpdateReturn(int v) { this->updateReturn_ = v; }

private:
    int startCount_ = 0;
    int stopCount_ = 0;
    bool moving_ = false;
    bool started_ = false;
    int updateReturn_ = 0;
};

class CoverUpdateTest : public EspTestBase {
public:
    static constexpr uint8_t stopPin = 50;

    // Context (state + config + services). Owns all mutable state.
    CoverMovementContext ctx;

    // Test doubles
    FakeCoverMovement up;
    FakeCoverMovement down;

    // Stopper (real, using FakeEspApi from base)
    CoverStop stopper{this->esp, this->stopPin, true,
                      false,     this->debug,   "[test] "};

    CoverUpdate updateImpl{this->ctx, this->up, this->down, this->stopper};
    InterfaceConfig config;
    Actions actions{this->config};

    CoverUpdateTest()
        : ctx{
              0,            // position
              false,        // stateChanged
              -1,           // activePositionSensor
              -2,           // previouslyActivePositionSensor
              0,            // previousMovementDirection
              -1,           // targetPosition
              0,            // restartCount
              {},           // positionSensors
              false,        // invertInput
              false,        // invertOutput
              false,        // invertPositionSensors
              10,           // closedPosition
              1,            // positionId
              this->esp,    // esp
              this->rtc,    // rtc
              this->debug,  // debug
              "[test] ",    // debugPrefix
          } {
        // Latching CoverStop constructor calls stop(), so reset it
        this->stopper.reset();
    }

    // Helper: check that storedValue has at least n entries, then return ref
    const std::string& valueAt(size_t n) {
        EXPECT_GE(this->config.storedValue.size(), n + 1)
            << "Expected at least " << (n + 1) << " storedValue entries, got "
            << this->config.storedValue.size();
        return this->config.storedValue[n];
    }
};

// ===== 1. Position sensor reading =====

TEST_F(CoverUpdateTest, UpdateReadsPositionSensors) {
    this->ctx.positionSensors.push_back({50, 5, false});
    this->esp.digitalWrite(5, 1);
    this->esp.delay(10);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.activePositionSensor, 0);
    EXPECT_EQ(this->ctx.position, 50);
}

TEST_F(CoverUpdateTest, UpdateDetectsSensorDeactivation) {
    this->ctx.positionSensors.push_back({50, 5, false});
    this->ctx.activePositionSensor = 0;

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.activePositionSensor, -1);
    EXPECT_EQ(this->ctx.previouslyActivePositionSensor, 0);
}

TEST_F(CoverUpdateTest, UpdateHandlesInvertedSensors) {
    this->ctx.positionSensors.push_back({50, 5, false});
    this->ctx.invertPositionSensors = true;
    this->esp.digitalWrite(5, 0);
    this->esp.delay(10);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.activePositionSensor, 0);
    EXPECT_EQ(this->ctx.position, 50);
}

// ===== 2. Position resolution =====

TEST_F(CoverUpdateTest, UpdateResolvesConflictingMovements) {
    this->ctx.position = 0;
    this->up.setUpdateReturn(10);
    this->down.setUpdateReturn(20);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.position, -1);
    EXPECT_EQ(this->up.stopCount(), 1);
    EXPECT_EQ(this->down.stopCount(), 1);
    // With position=-1, only state name is fired (no position value)
    ASSERT_EQ(this->config.storedValue.size(), 1u);
    // -1 <= closedPosition (10) → CLOSED
    EXPECT_EQ(this->valueAt(0), "CLOSED");
}

TEST_F(CoverUpdateTest, UpdateUsesUpPosition) {
    this->ctx.position = 0;
    this->up.setUpdateReturn(10);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.position, 10);
}

TEST_F(CoverUpdateTest, UpdateUsesDownPosition) {
    this->ctx.position = 0;
    this->down.setUpdateReturn(10);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.position, 10);
}

TEST_F(CoverUpdateTest, UpdateOverridesPositionFromSensor) {
    this->ctx.positionSensors.push_back({75, 5, false});
    this->ctx.activePositionSensor = 0;
    this->esp.digitalWrite(5, 1);
    this->esp.delay(10);
    // up returns a different position, but sensor should override
    this->up.setUpdateReturn(10);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.position, 75);
}

// ===== 3. Movement direction =====

TEST_F(CoverUpdateTest, UpdateDetectsOpeningDirection) {
    this->ctx.position = 0;
    this->up.setMoving(true);
    this->up.setUpdateReturn(10);
    this->down.setUpdateReturn(0);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.previousMovementDirection, 1);
    // stateChanged is cleared after action fires; check it via the fact that
    // action was fired despite no position change (direction change triggered
    // it)
    EXPECT_EQ(this->valueAt(0), "OPENING");
}

TEST_F(CoverUpdateTest, UpdateDetectsClosingDirection) {
    this->ctx.position = 100;
    this->down.setMoving(true);
    this->down.setUpdateReturn(50);
    // Keep up.update() returning same as position to avoid conflict
    this->up.setUpdateReturn(100);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.previousMovementDirection, -1);
    EXPECT_EQ(this->valueAt(0), "CLOSING");
}

TEST_F(CoverUpdateTest, UpdateDetectsStopped) {
    this->ctx.position = 50;
    this->ctx.previousMovementDirection = 0;
    this->ctx.stateChanged = false;
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(50);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.previousMovementDirection, 0);
    EXPECT_FALSE(this->ctx.stateChanged);
    EXPECT_TRUE(this->config.storedValue.empty());
}

TEST_F(CoverUpdateTest, UpdateSetsStateChangedOnDirectionChange) {
    this->ctx.previousMovementDirection = -1;  // Was closing
    this->ctx.position = 0;
    this->up.setMoving(true);
    this->up.setUpdateReturn(10);
    this->down.setUpdateReturn(0);

    this->updateImpl.update(this->actions);

    // After update(), stateChanged is cleared by the emission block. But the
    // fact that an action fired despite position not changing shows
    // stateChanged was true during this cycle.
    EXPECT_FALSE(this->ctx.stateChanged);
    EXPECT_EQ(this->ctx.previousMovementDirection, 1);
    EXPECT_EQ(this->valueAt(0), "OPENING");
}

// ===== 4. State emission =====

TEST_F(CoverUpdateTest, UpdateFiresOpeningState) {
    this->ctx.position = 0;
    this->up.setMoving(true);
    this->up.setUpdateReturn(10);
    this->down.setUpdateReturn(0);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->valueAt(0), "OPENING");
    EXPECT_EQ(this->valueAt(1), "10");
}

TEST_F(CoverUpdateTest, UpdateFiresClosingState) {
    this->ctx.position = 100;
    this->down.setMoving(true);
    this->down.setUpdateReturn(50);
    this->up.setUpdateReturn(100);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->valueAt(0), "CLOSING");
    EXPECT_EQ(this->valueAt(1), "50");
}

TEST_F(CoverUpdateTest, UpdateFiresClosedState) {
    this->ctx.position = 0;
    this->ctx.closedPosition = 10;
    // Movement returns new position 5 (<= closedPosition 10)
    this->up.setUpdateReturn(5);
    this->down.setUpdateReturn(0);  // Same as old position, not conflicting

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->valueAt(0), "CLOSED");
    EXPECT_EQ(this->valueAt(1), "5");
}

TEST_F(CoverUpdateTest, UpdateFiresOpenState) {
    this->ctx.position = 10;
    this->ctx.closedPosition = 10;
    // Movement returns new position 50 (> closedPosition 10)
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(10);  // Same as old position, not conflicting

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->valueAt(0), "OPEN");
    EXPECT_EQ(this->valueAt(1), "50");
}

TEST_F(CoverUpdateTest, UpdateOmitsPositionWhenNoPosition) {
    this->ctx.position = 0;
    this->up.setUpdateReturn(10);
    this->down.setUpdateReturn(20);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.position, -1);
    // Only state name, no position value
    EXPECT_EQ(this->valueAt(0), "CLOSED");
}

TEST_F(CoverUpdateTest, UpdateDoesNotFireWhenUnchanged) {
    this->ctx.position = 50;
    this->ctx.previousMovementDirection = 0;
    this->ctx.stateChanged = false;
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(50);

    this->updateImpl.update(this->actions);

    EXPECT_TRUE(this->config.storedValue.empty());
    EXPECT_EQ(this->ctx.position, 50);
    EXPECT_FALSE(this->ctx.stateChanged);
}

// ===== 5. Stopper handling =====

TEST_F(CoverUpdateTest, UpdateResetsStopperWhenStopped) {
    // Manually trigger the stopper
    this->stopper.stop();
    EXPECT_TRUE(this->stopper.isTriggered());

    // Neither moving → stopper.reset() will be called during update
    this->updateImpl.update(this->actions);

    EXPECT_FALSE(this->stopper.isTriggered());
}

TEST_F(CoverUpdateTest, UpdateKeepsStopperActive) {
    this->up.setMoving(true);
    this->up.setUpdateReturn(10);
    this->down.setUpdateReturn(0);
    this->stopper.stop();
    EXPECT_TRUE(this->stopper.isTriggered());

    this->updateImpl.update(this->actions);

    // Still moving, so stopper should remain triggered
    EXPECT_TRUE(this->stopper.isTriggered());
}

// ===== 6. Target position handling =====

TEST_F(CoverUpdateTest, UpdateClearsTargetWhenReached) {
    this->ctx.targetPosition = 50;
    this->ctx.position = 50;
    this->ctx.previousMovementDirection = 1;
    // Keep moving so stopper reset logic doesn't interact
    this->up.setMoving(true);
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(50);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.targetPosition, -1);
    EXPECT_EQ(this->ctx.restartCount, 0u);
}

TEST_F(CoverUpdateTest, UpdateRestartsWhenBothMovementsIdle) {
    this->ctx.targetPosition = 75;
    this->ctx.position = 50;
    this->ctx.previousMovementDirection = 0;
    this->ctx.positionSensors.clear();
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(50);

    // targetPosition(75) > position(50) → up.start(), down.stop()
    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.restartCount, 1u);
    EXPECT_EQ(this->up.startCount(), 1);
    EXPECT_EQ(this->down.stopCount(), 1);
    EXPECT_EQ(this->up.stopCount(), 0);
    EXPECT_EQ(this->down.startCount(), 0);
}

TEST_F(CoverUpdateTest, UpdateStopsRestartingAfterMaxAttempts) {
    this->ctx.targetPosition = 75;
    this->ctx.position = 50;
    this->ctx.restartCount = 3;
    this->ctx.previousMovementDirection = 0;
    this->ctx.positionSensors.clear();
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(50);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.targetPosition, -1);
    EXPECT_EQ(this->ctx.restartCount, 0u);
    EXPECT_EQ(this->up.stopCount(), 1);
    EXPECT_EQ(this->down.stopCount(), 1);
}

TEST_F(CoverUpdateTest, UpdateSkipsRestartWithPositionSensors) {
    this->ctx.targetPosition = 75;
    this->ctx.position = 50;
    this->ctx.previousMovementDirection = 0;
    this->ctx.positionSensors.push_back({50, 5, false});
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(50);

    this->updateImpl.update(this->actions);

    // Has position sensors and position != 0/100 → Action::Reset
    EXPECT_EQ(this->ctx.targetPosition, -1);
    EXPECT_EQ(this->ctx.restartCount, 0u);
    EXPECT_EQ(this->up.stopCount(), 1);
    EXPECT_EQ(this->down.stopCount(), 1);
}

TEST_F(CoverUpdateTest, UpdateIncrementsRestartCount) {
    this->ctx.targetPosition = 75;
    this->ctx.position = 50;
    this->ctx.previousMovementDirection = 0;
    this->ctx.positionSensors.clear();
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(50);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.restartCount, 1u);
}

TEST_F(CoverUpdateTest, UpdateAllowsRestartWithSensorsAtBounds) {
    // With position sensors but position is 0 (closed boundary), restart is
    // allowed because position is 0 (known position).
    this->ctx.targetPosition = 75;
    this->ctx.position = 0;
    this->ctx.previousMovementDirection = 0;
    this->ctx.positionSensors.push_back({0, 5, false});
    this->up.setUpdateReturn(0);
    this->down.setUpdateReturn(0);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.restartCount, 1u);
    EXPECT_EQ(this->up.startCount(), 1);
    EXPECT_EQ(this->ctx.targetPosition, 75);  // Not cleared
}

TEST_F(CoverUpdateTest, UpdateAllowsRestartWithSensorsAtOpenBoundary) {
    // With position sensors but position is 100 (open boundary), restart is
    // allowed because position is 100 (known position).
    this->ctx.targetPosition = 50;
    this->ctx.position = 100;
    this->ctx.previousMovementDirection = 0;
    this->ctx.positionSensors.push_back({100, 5, false});
    this->up.setUpdateReturn(100);
    this->down.setUpdateReturn(100);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->ctx.restartCount, 1u);
    EXPECT_EQ(this->down.startCount(), 1);
    EXPECT_EQ(this->ctx.targetPosition, 50);  // Not cleared
}

TEST_F(CoverUpdateTest, UpdateRestartsInCorrectDirectionDown) {
    // targetPosition < position → should start down, stop up
    this->ctx.targetPosition = 25;
    this->ctx.position = 50;
    this->ctx.previousMovementDirection = 0;
    this->ctx.positionSensors.clear();
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(50);

    this->updateImpl.update(this->actions);

    EXPECT_EQ(this->down.startCount(), 1);
    EXPECT_EQ(this->up.stopCount(), 1);
    EXPECT_EQ(this->up.startCount(), 0);
    EXPECT_EQ(this->down.stopCount(), 0);
}

// ===== 7. RTC persistence =====

TEST_F(CoverUpdateTest, UpdatePersistsPositionToRtc) {
    this->ctx.position = 0;
    this->up.setUpdateReturn(42);
    this->down.setUpdateReturn(0);

    this->updateImpl.update(this->actions);

    // rtc.set(positionId, position + 1)
    EXPECT_EQ(this->rtc.get(this->ctx.positionId), 43u);
}
