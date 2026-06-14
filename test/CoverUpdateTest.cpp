#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "EspTestBase.hpp"
#include "common/Actions.hpp"
#include "common/CoverMovement.hpp"
#include "common/CoverState.hpp"
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

// Hand-rolled test double for CoverStop.
// Tracks stop/reset calls and allows configuring behavior.
class FakeCoverStop : public CoverStop {
public:
    void stop() override {
        ++this->stopCount_;
        this->triggered_ = true;
    }
    void reset() override {
        ++this->resetCount_;
        this->triggered_ = false;
    }
    bool isTriggered() const override { return this->triggered_; }
    bool isLatching() const override { return this->latching_; }

    void setLatching(bool v) { this->latching_ = v; }
    int stopCount() const { return this->stopCount_; }
    int resetCount() const { return this->resetCount_; }

private:
    bool triggered_ = false;
    bool latching_ = false;
    int stopCount_ = 0;
    int resetCount_ = 0;
};

class CoverUpdateTest : public EspTestBase {
public:
    static constexpr uint8_t stopPin = 50;

    // Context (state + config + services). Owns all mutable state.
    CoverState ctx;

    // Raw pointers to fakes (owned by unique_ptrs inside update)
    FakeCoverMovement* upPtr;
    FakeCoverMovement* downPtr;
    FakeCoverStop* stopperPtr;

    std::unique_ptr<CoverUpdate> update;
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
        auto up = std::make_unique<FakeCoverMovement>();
        auto down = std::make_unique<FakeCoverMovement>();
        auto stopper = std::make_unique<FakeCoverStop>();
        this->upPtr = up.get();
        this->downPtr = down.get();
        this->stopperPtr = stopper.get();
        this->update = std::make_unique<CoverUpdate>(
            this->ctx, std::move(up), std::move(down), std::move(stopper));
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

    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.activePositionSensor, 0);
    EXPECT_EQ(this->ctx.position, 50);
}

TEST_F(CoverUpdateTest, UpdateDetectsSensorDeactivation) {
    this->ctx.positionSensors.push_back({50, 5, false});
    this->ctx.activePositionSensor = 0;

    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.activePositionSensor, -1);
    EXPECT_EQ(this->ctx.previouslyActivePositionSensor, 0);
}

TEST_F(CoverUpdateTest, UpdateHandlesInvertedSensors) {
    this->ctx.positionSensors.push_back({50, 5, false});
    this->ctx.invertPositionSensors = true;
    this->esp.digitalWrite(5, 0);
    this->esp.delay(10);

    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.activePositionSensor, 0);
    EXPECT_EQ(this->ctx.position, 50);
}

// ===== 2. Position resolution =====

TEST_F(CoverUpdateTest, UpdateResolvesConflictingMovements) {
    this->ctx.position = 0;
    this->upPtr->setUpdateReturn(10);
    this->downPtr->setUpdateReturn(20);

    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.position, -1);
    EXPECT_EQ(this->upPtr->stopCount(), 1);
    EXPECT_EQ(this->downPtr->stopCount(), 1);
    // With position=-1, only state name is fired (no position value)
    ASSERT_EQ(this->config.storedValue.size(), 1u);
    // -1 <= closedPosition (10) → CLOSED
    EXPECT_EQ(this->valueAt(0), "CLOSED");
}

TEST_F(CoverUpdateTest, UpdateUsesUpPosition) {
    this->ctx.position = 0;
    this->upPtr->setUpdateReturn(10);

    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.position, 10);
}

TEST_F(CoverUpdateTest, UpdateUsesDownPosition) {
    this->ctx.position = 0;
    this->downPtr->setUpdateReturn(10);

    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.position, 10);
}

TEST_F(CoverUpdateTest, UpdateOverridesPositionFromSensor) {
    this->ctx.positionSensors.push_back({75, 5, false});
    this->ctx.activePositionSensor = 0;
    this->esp.digitalWrite(5, 1);
    this->esp.delay(10);
    // up returns a different position, but sensor should override
    this->upPtr->setUpdateReturn(10);

    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.position, 75);
}

// ===== 3. Movement direction =====

TEST_F(CoverUpdateTest, UpdateDetectsOpeningDirection) {
    this->ctx.position = 0;
    this->upPtr->setMoving(true);
    this->upPtr->setUpdateReturn(10);
    this->downPtr->setUpdateReturn(0);

    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.previousMovementDirection, 1);
    // stateChanged is cleared after action fires; check it via the fact that
    // action was fired despite no position change (direction change triggered
    // it)
    EXPECT_EQ(this->valueAt(0), "OPENING");
}

TEST_F(CoverUpdateTest, UpdateDetectsClosingDirection) {
    this->ctx.position = 100;
    this->downPtr->setMoving(true);
    this->downPtr->setUpdateReturn(50);
    // Keep up.update() returning same as position to avoid conflict
    this->upPtr->setUpdateReturn(100);

    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.previousMovementDirection, -1);
    EXPECT_EQ(this->valueAt(0), "CLOSING");
}

TEST_F(CoverUpdateTest, UpdateDetectsStopped) {
    this->ctx.position = 50;
    this->ctx.previousMovementDirection = 0;
    this->ctx.stateChanged = false;
    this->upPtr->setUpdateReturn(50);
    this->downPtr->setUpdateReturn(50);

    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.previousMovementDirection, 0);
    EXPECT_FALSE(this->ctx.stateChanged);
    EXPECT_TRUE(this->config.storedValue.empty());
}

TEST_F(CoverUpdateTest, UpdateSetsStateChangedOnDirectionChange) {
    this->ctx.previousMovementDirection = -1;  // Was closing
    this->ctx.position = 0;
    this->upPtr->setMoving(true);
    this->upPtr->setUpdateReturn(10);
    this->downPtr->setUpdateReturn(0);

    this->update->update(this->actions);

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
    this->upPtr->setMoving(true);
    this->upPtr->setUpdateReturn(10);
    this->downPtr->setUpdateReturn(0);

    this->update->update(this->actions);

    EXPECT_EQ(this->valueAt(0), "OPENING");
    EXPECT_EQ(this->valueAt(1), "10");
}

TEST_F(CoverUpdateTest, UpdateFiresClosingState) {
    this->ctx.position = 100;
    this->downPtr->setMoving(true);
    this->downPtr->setUpdateReturn(50);
    this->upPtr->setUpdateReturn(100);

    this->update->update(this->actions);

    EXPECT_EQ(this->valueAt(0), "CLOSING");
    EXPECT_EQ(this->valueAt(1), "50");
}

TEST_F(CoverUpdateTest, UpdateFiresClosedState) {
    this->ctx.position = 0;
    this->ctx.closedPosition = 10;
    // Movement returns new position 5 (<= closedPosition 10)
    this->upPtr->setUpdateReturn(5);
    this->downPtr->setUpdateReturn(0);  // Same as old position, not conflicting

    this->update->update(this->actions);

    EXPECT_EQ(this->valueAt(0), "CLOSED");
    EXPECT_EQ(this->valueAt(1), "5");
}

TEST_F(CoverUpdateTest, UpdateFiresOpenState) {
    this->ctx.position = 10;
    this->ctx.closedPosition = 10;
    // Movement returns new position 50 (> closedPosition 10)
    this->upPtr->setUpdateReturn(50);
    this->downPtr->setUpdateReturn(
        10);  // Same as old position, not conflicting

    this->update->update(this->actions);

    EXPECT_EQ(this->valueAt(0), "OPEN");
    EXPECT_EQ(this->valueAt(1), "50");
}

TEST_F(CoverUpdateTest, UpdateOmitsPositionWhenNoPosition) {
    this->ctx.position = 0;
    this->upPtr->setUpdateReturn(10);
    this->downPtr->setUpdateReturn(20);

    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.position, -1);
    // Only state name, no position value
    EXPECT_EQ(this->valueAt(0), "CLOSED");
}

TEST_F(CoverUpdateTest, UpdateDoesNotFireWhenUnchanged) {
    this->ctx.position = 50;
    this->ctx.previousMovementDirection = 0;
    this->ctx.stateChanged = false;
    this->upPtr->setUpdateReturn(50);
    this->downPtr->setUpdateReturn(50);

    this->update->update(this->actions);

    EXPECT_TRUE(this->config.storedValue.empty());
    EXPECT_EQ(this->ctx.position, 50);
    EXPECT_FALSE(this->ctx.stateChanged);
}

// ===== 5. Stopper handling =====

TEST_F(CoverUpdateTest, UpdateResetsStopperWhenStopped) {
    // Manually trigger the stopper
    this->stopperPtr->stop();
    EXPECT_TRUE(this->stopperPtr->isTriggered());

    // Neither moving → stopper.reset() will be called during update
    this->update->update(this->actions);

    EXPECT_FALSE(this->stopperPtr->isTriggered());
}

TEST_F(CoverUpdateTest, UpdateKeepsStopperActive) {
    this->upPtr->setMoving(true);
    this->upPtr->setUpdateReturn(10);
    this->downPtr->setUpdateReturn(0);
    this->stopperPtr->stop();
    EXPECT_TRUE(this->stopperPtr->isTriggered());

    this->update->update(this->actions);

    // Still moving, so stopper should remain triggered
    EXPECT_TRUE(this->stopperPtr->isTriggered());
}

// ===== 6. Target position handling =====

TEST_F(CoverUpdateTest, UpdateClearsTargetWhenReached) {
    this->ctx.targetPosition = 50;
    this->ctx.position = 50;
    this->ctx.previousMovementDirection = 1;
    // Keep moving so stopper reset logic doesn't interact
    this->upPtr->setMoving(true);
    this->upPtr->setUpdateReturn(50);
    this->downPtr->setUpdateReturn(50);

    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.targetPosition, -1);
    EXPECT_EQ(this->ctx.restartCount, 0u);
}

TEST_F(CoverUpdateTest, UpdateRestartsWhenBothMovementsIdle) {
    this->ctx.targetPosition = 75;
    this->ctx.position = 50;
    this->ctx.previousMovementDirection = 0;
    this->ctx.positionSensors.clear();
    this->upPtr->setUpdateReturn(50);
    this->downPtr->setUpdateReturn(50);

    // targetPosition(75) > position(50) → up.start(), down.stop()
    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.restartCount, 1u);
    EXPECT_EQ(this->upPtr->startCount(), 1);
    EXPECT_EQ(this->downPtr->stopCount(), 1);
    EXPECT_EQ(this->upPtr->stopCount(), 0);
    EXPECT_EQ(this->downPtr->startCount(), 0);
}

TEST_F(CoverUpdateTest, UpdateStopsRestartingAfterMaxAttempts) {
    this->ctx.targetPosition = 75;
    this->ctx.position = 50;
    this->ctx.restartCount = 3;
    this->ctx.previousMovementDirection = 0;
    this->ctx.positionSensors.clear();
    this->upPtr->setUpdateReturn(50);
    this->downPtr->setUpdateReturn(50);

    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.targetPosition, -1);
    EXPECT_EQ(this->ctx.restartCount, 0u);
    EXPECT_EQ(this->upPtr->stopCount(), 1);
    EXPECT_EQ(this->downPtr->stopCount(), 1);
}

TEST_F(CoverUpdateTest, UpdateSkipsRestartWithPositionSensors) {
    this->ctx.targetPosition = 75;
    this->ctx.position = 50;
    this->ctx.previousMovementDirection = 0;
    this->ctx.positionSensors.push_back({50, 5, false});
    this->upPtr->setUpdateReturn(50);
    this->downPtr->setUpdateReturn(50);

    this->update->update(this->actions);

    // Has position sensors and position != 0/100 → Action::Reset
    EXPECT_EQ(this->ctx.targetPosition, -1);
    EXPECT_EQ(this->ctx.restartCount, 0u);
    EXPECT_EQ(this->upPtr->stopCount(), 1);
    EXPECT_EQ(this->downPtr->stopCount(), 1);
}

TEST_F(CoverUpdateTest, UpdateIncrementsRestartCount) {
    this->ctx.targetPosition = 75;
    this->ctx.position = 50;
    this->ctx.previousMovementDirection = 0;
    this->ctx.positionSensors.clear();
    this->upPtr->setUpdateReturn(50);
    this->downPtr->setUpdateReturn(50);

    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.restartCount, 1u);
}

TEST_F(CoverUpdateTest, UpdateAllowsRestartWithSensorsAtBounds) {
    // With position sensors but position is 0 (closed boundary), restart is
    // allowed because position is 0 (known position).
    this->ctx.targetPosition = 75;
    this->ctx.position = 0;
    this->ctx.previousMovementDirection = 0;
    this->ctx.positionSensors.push_back({0, 5, false});
    this->upPtr->setUpdateReturn(0);
    this->downPtr->setUpdateReturn(0);

    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.restartCount, 1u);
    EXPECT_EQ(this->upPtr->startCount(), 1);
    EXPECT_EQ(this->ctx.targetPosition, 75);  // Not cleared
}

TEST_F(CoverUpdateTest, UpdateAllowsRestartWithSensorsAtOpenBoundary) {
    // With position sensors but position is 100 (open boundary), restart is
    // allowed because position is 100 (known position).
    this->ctx.targetPosition = 50;
    this->ctx.position = 100;
    this->ctx.previousMovementDirection = 0;
    this->ctx.positionSensors.push_back({100, 5, false});
    this->upPtr->setUpdateReturn(100);
    this->downPtr->setUpdateReturn(100);

    this->update->update(this->actions);

    EXPECT_EQ(this->ctx.restartCount, 1u);
    EXPECT_EQ(this->downPtr->startCount(), 1);
    EXPECT_EQ(this->ctx.targetPosition, 50);  // Not cleared
}

TEST_F(CoverUpdateTest, UpdateRestartsInCorrectDirectionDown) {
    // targetPosition < position → should start down, stop up
    this->ctx.targetPosition = 25;
    this->ctx.position = 50;
    this->ctx.previousMovementDirection = 0;
    this->ctx.positionSensors.clear();
    this->upPtr->setUpdateReturn(50);
    this->downPtr->setUpdateReturn(50);

    this->update->update(this->actions);

    EXPECT_EQ(this->downPtr->startCount(), 1);
    EXPECT_EQ(this->upPtr->stopCount(), 1);
    EXPECT_EQ(this->upPtr->startCount(), 0);
    EXPECT_EQ(this->downPtr->stopCount(), 0);
}

// ===== 8. requestOpen =====

TEST_F(CoverUpdateTest, RequestOpenStartsUpAndStopsDown) {
    this->update->requestOpen();
    EXPECT_EQ(this->upPtr->startCount(), 1);
    EXPECT_EQ(this->downPtr->stopCount(), 1);
    EXPECT_TRUE(this->ctx.stateChanged);
    EXPECT_EQ(this->ctx.targetPosition, -1);
}

TEST_F(CoverUpdateTest, RequestOpenIsIdempotentWhenUpAlreadyStarted) {
    this->upPtr->setStarted(true);
    this->update->requestOpen();
    EXPECT_EQ(this->upPtr->startCount(), 0);
    EXPECT_FALSE(this->ctx.stateChanged);
}

// ===== 9. requestClose =====

TEST_F(CoverUpdateTest, RequestCloseStartsDownAndStopsUp) {
    this->update->requestClose();
    EXPECT_EQ(this->downPtr->startCount(), 1);
    EXPECT_EQ(this->upPtr->stopCount(), 1);
    EXPECT_TRUE(this->ctx.stateChanged);
    EXPECT_EQ(this->ctx.targetPosition, -1);
}

TEST_F(CoverUpdateTest, RequestCloseIsIdempotentWhenDownAlreadyStarted) {
    this->downPtr->setStarted(true);
    this->update->requestClose();
    EXPECT_EQ(this->downPtr->startCount(), 0);
    EXPECT_FALSE(this->ctx.stateChanged);
}

// ===== 10. requestStop =====

TEST_F(CoverUpdateTest, RequestStopStopsAll) {
    this->update->requestStop();
    EXPECT_EQ(this->upPtr->stopCount(), 1);
    EXPECT_EQ(this->downPtr->stopCount(), 1);
    EXPECT_EQ(this->stopperPtr->stopCount(), 1);
    EXPECT_EQ(this->ctx.targetPosition, -1);
}

// ===== 11. requestSetPosition =====

TEST_F(CoverUpdateTest, RequestSetPositionRejectsOutOfRange) {
    this->ctx.position = 50;
    this->update->requestSetPosition(-1);
    EXPECT_EQ(this->ctx.targetPosition, -1);
    EXPECT_EQ(this->upPtr->startCount(), 0);
    EXPECT_EQ(this->downPtr->startCount(), 0);

    this->ctx.targetPosition = -1;
    this->update->requestSetPosition(101);
    EXPECT_EQ(this->ctx.targetPosition, -1);
    EXPECT_EQ(this->upPtr->startCount(), 0);
    EXPECT_EQ(this->downPtr->startCount(), 0);
}

TEST_F(CoverUpdateTest, RequestSetPositionGreaterThanCurrentOpens) {
    this->ctx.position = 30;
    this->update->requestSetPosition(70);
    EXPECT_EQ(this->ctx.targetPosition, 70);
    EXPECT_EQ(this->ctx.restartCount, 0u);
    EXPECT_EQ(this->upPtr->startCount(), 1);
    EXPECT_EQ(this->downPtr->stopCount(), 1);
}

TEST_F(CoverUpdateTest, RequestSetPositionLessThanCurrentCloses) {
    this->ctx.position = 70;
    this->update->requestSetPosition(30);
    EXPECT_EQ(this->ctx.targetPosition, 30);
    EXPECT_EQ(this->downPtr->startCount(), 1);
    EXPECT_EQ(this->upPtr->stopCount(), 1);
}

TEST_F(CoverUpdateTest, RequestSetPositionEqualToCurrentStops) {
    this->ctx.position = 50;
    this->update->requestSetPosition(50);
    EXPECT_EQ(this->ctx.targetPosition, 50);
    EXPECT_EQ(this->upPtr->stopCount(), 1);
    EXPECT_EQ(this->downPtr->stopCount(), 1);
    EXPECT_EQ(this->stopperPtr->stopCount(), 1);
}

TEST_F(CoverUpdateTest, RequestSetPositionAtBoundaryOpen) {
    this->ctx.position = 30;
    this->update->requestSetPosition(100);
    EXPECT_EQ(this->upPtr->startCount(), 1);
}

TEST_F(CoverUpdateTest, RequestSetPositionAtBoundaryClosed) {
    this->ctx.position = 30;
    this->update->requestSetPosition(0);
    EXPECT_EQ(this->downPtr->startCount(), 1);
}

TEST_F(CoverUpdateTest, RequestSetPositionResetsRestartCount) {
    this->ctx.position = 30;
    this->ctx.restartCount = 2;
    this->update->requestSetPosition(70);
    EXPECT_EQ(this->ctx.restartCount, 0u);
}

// ===== 7. RTC persistence =====

TEST_F(CoverUpdateTest, UpdatePersistsPositionToRtc) {
    this->ctx.position = 0;
    this->upPtr->setUpdateReturn(42);
    this->downPtr->setUpdateReturn(0);

    this->update->update(this->actions);

    // rtc.set(positionId, position + 1)
    EXPECT_EQ(this->rtc.get(this->ctx.positionId), 43u);
}
