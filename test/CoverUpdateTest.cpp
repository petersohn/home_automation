#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <vector>

#include "EspTestBase.hpp"
#include "common/Actions.hpp"
#include "common/CoverMovement.hpp"
#include "common/CoverMovementContext.hpp"
#include "common/CoverStop.hpp"
#include "common/CoverUpdateImpl.hpp"
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

    void reset() {
        this->startCount_ = 0;
        this->stopCount_ = 0;
        this->moving_ = false;
        this->started_ = false;
        this->updateReturn_ = 0;
    }

private:
    int startCount_ = 0;
    int stopCount_ = 0;
    bool moving_ = false;
    bool started_ = false;
    int updateReturn_ = 0;
};

// Helper to initialize aggregate. Do NOT use initializer list syntax because
// CoverMovementContext has const fields that would be copied, and test helper
// fields set before calling this are what should be captured.
static CoverMovementContext makeContext(
    int& position, bool& stateChanged, int& activePositionSensor,
    int& previouslyActivePositionSensor, int& previousMovementDirection,
    int& targetPosition, unsigned& restartCount,
    std::vector<PositionSensor>& positionSensors, bool invertInput,
    bool invertOutput, bool invertPositionSensors, int closedPosition,
    unsigned positionId, EspApi& esp, Rtc& rtc, std::ostream& debug,
    std::string& debugPrefix) {
    return CoverMovementContext{
        position,
        stateChanged,
        activePositionSensor,
        previouslyActivePositionSensor,
        previousMovementDirection,
        targetPosition,
        restartCount,
        positionSensors,
        invertInput,
        invertOutput,
        invertPositionSensors,
        closedPosition,
        positionId,
        esp,
        rtc,
        debug,
        debugPrefix};
}

class CoverUpdateTest : public EspTestBase {
public:
    // Mutable state referenced by CoverMovementContext
    int position = 0;
    bool stateChanged = false;
    int activePositionSensor = -1;
    int previouslyActivePositionSensor = -2;
    int previousMovementDirection = 0;
    int targetPosition = -1;
    unsigned restartCount = 0;

    // Config
    std::vector<PositionSensor> positionSensors;
    bool invertInput = false;
    bool invertOutput = false;
    bool invertPositionSensors = false;
    int closedPosition = 10;
    unsigned positionId = 1;

    // Debug
    std::string debugPrefix = "[test] ";

    // Test doubles
    FakeCoverMovement up;
    FakeCoverMovement down;

    // Stopper (real, using FakeEspApi from base)
    CoverStop stopper{this->esp, 50,          true,
                      false,     this->debug, this->debugPrefix};

    // Dynamically-built dependencies (rebuilt before each test via init())
    std::unique_ptr<CoverMovementContext> ctx;
    std::unique_ptr<CoverUpdateImpl> updateImpl;
    InterfaceConfig config;
    Actions actions{this->config};

    CoverUpdateTest() {
        // Latching CoverStop constructor calls stop(), so reset it
        this->stopper.reset();
    }

    // Call init() after setting up test-specific state to build the context.
    void init() {
        this->ctx = std::make_unique<CoverMovementContext>(makeContext(
            this->position, this->stateChanged, this->activePositionSensor,
            this->previouslyActivePositionSensor,
            this->previousMovementDirection, this->targetPosition,
            this->restartCount, this->positionSensors, this->invertInput,
            this->invertOutput, this->invertPositionSensors,
            this->closedPosition, this->positionId, this->esp, this->rtc,
            this->debug, this->debugPrefix));
        this->updateImpl = std::make_unique<CoverUpdateImpl>(
            *this->ctx, this->up, this->down, this->stopper);
    }

    void TearDown() override {
        this->up.reset();
        this->down.reset();
        // Reset stopper for next test - also resets in constructor body
        // but teardown doesn't re-run constructor, so do it here too.
        this->stopper.reset();
    }
};

// ===== 1. Position sensor reading =====

TEST_F(CoverUpdateTest, UpdateReadsPositionSensors) {
    this->positionSensors.push_back({50, 5, false});
    this->esp.digitalWrite(5, 1);
    this->esp.delay(10);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->activePositionSensor, 0);
    EXPECT_EQ(this->ctx->position, 50);
}

TEST_F(CoverUpdateTest, UpdateDetectsSensorDeactivation) {
    this->positionSensors.push_back({50, 5, false});
    this->activePositionSensor = 0;
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->activePositionSensor, -1);
    EXPECT_EQ(this->ctx->previouslyActivePositionSensor, 0);
}

TEST_F(CoverUpdateTest, UpdateHandlesInvertedSensors) {
    this->positionSensors.push_back({50, 5, false});
    this->invertPositionSensors = true;
    this->esp.digitalWrite(5, 0);
    this->esp.delay(10);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->activePositionSensor, 0);
    EXPECT_EQ(this->ctx->position, 50);
}

// ===== 2. Position resolution =====

TEST_F(CoverUpdateTest, UpdateResolvesConflictingMovements) {
    this->position = 0;
    this->up.setUpdateReturn(10);
    this->down.setUpdateReturn(20);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->position, -1);
    EXPECT_EQ(this->up.stopCount(), 1);
    EXPECT_EQ(this->down.stopCount(), 1);
    // With position=-1, only state name is fired (no position value)
    ASSERT_EQ(this->config.storedValue.size(), 1u);
    // -1 <= closedPosition (10) → CLOSED
    EXPECT_EQ(this->config.storedValue[0], "CLOSED");
}

TEST_F(CoverUpdateTest, UpdateUsesUpPosition) {
    this->position = 0;
    this->up.setUpdateReturn(10);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->position, 10);
}

TEST_F(CoverUpdateTest, UpdateUsesDownPosition) {
    this->position = 0;
    this->down.setUpdateReturn(10);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->position, 10);
}

TEST_F(CoverUpdateTest, UpdateOverridesPositionFromSensor) {
    this->positionSensors.push_back({75, 5, false});
    this->activePositionSensor = 0;
    this->esp.digitalWrite(5, 1);
    this->esp.delay(10);
    // up returns a different position, but sensor should override
    this->up.setUpdateReturn(10);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->position, 75);
}

// ===== 3. Movement direction =====

TEST_F(CoverUpdateTest, UpdateDetectsOpeningDirection) {
    this->position = 0;
    this->up.setMoving(true);
    this->up.setUpdateReturn(10);
    this->down.setUpdateReturn(0);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->previousMovementDirection, 1);
    // stateChanged is cleared after action fires; check it via the fact that
    // action was fired despite no position change (direction change triggered
    // it)
    EXPECT_EQ(this->config.storedValue[0], "OPENING");
}

TEST_F(CoverUpdateTest, UpdateDetectsClosingDirection) {
    this->position = 100;
    this->down.setMoving(true);
    this->down.setUpdateReturn(50);
    // Keep up.update() returning same as position to avoid conflict
    this->up.setUpdateReturn(100);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->previousMovementDirection, -1);
    EXPECT_EQ(this->config.storedValue[0], "CLOSING");
}

TEST_F(CoverUpdateTest, UpdateDetectsStopped) {
    this->position = 50;
    this->previousMovementDirection = 0;
    this->stateChanged = false;
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(50);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->previousMovementDirection, 0);
    EXPECT_FALSE(this->ctx->stateChanged);
    EXPECT_TRUE(this->config.storedValue.empty());
}

TEST_F(CoverUpdateTest, UpdateSetsStateChangedOnDirectionChange) {
    this->previousMovementDirection = -1;  // Was closing
    this->position = 0;
    this->up.setMoving(true);
    this->up.setUpdateReturn(10);
    this->down.setUpdateReturn(0);
    this->init();

    this->updateImpl->update(this->actions);

    // After update(), stateChanged is cleared by the emission block. But the
    // fact that an action fired despite position not changing shows
    // stateChanged was true during this cycle.
    EXPECT_FALSE(this->ctx->stateChanged);
    EXPECT_EQ(this->ctx->previousMovementDirection, 1);
    EXPECT_EQ(this->config.storedValue[0], "OPENING");
}

// ===== 4. State emission =====

TEST_F(CoverUpdateTest, UpdateFiresOpeningState) {
    this->position = 0;
    this->up.setMoving(true);
    this->up.setUpdateReturn(10);
    this->down.setUpdateReturn(0);
    this->init();

    this->updateImpl->update(this->actions);

    ASSERT_GE(this->config.storedValue.size(), 2u);
    EXPECT_EQ(this->config.storedValue[0], "OPENING");
    EXPECT_EQ(this->config.storedValue[1], "10");
}

TEST_F(CoverUpdateTest, UpdateFiresClosingState) {
    this->position = 100;
    this->down.setMoving(true);
    this->down.setUpdateReturn(50);
    this->up.setUpdateReturn(100);
    this->init();

    this->updateImpl->update(this->actions);

    ASSERT_GE(this->config.storedValue.size(), 2u);
    EXPECT_EQ(this->config.storedValue[0], "CLOSING");
    EXPECT_EQ(this->config.storedValue[1], "50");
}

TEST_F(CoverUpdateTest, UpdateFiresClosedState) {
    this->position = 0;
    this->closedPosition = 10;
    // Movement returns new position 5 (<= closedPosition 10)
    this->up.setUpdateReturn(5);
    this->down.setUpdateReturn(0);  // Same as old position, not conflicting
    this->init();

    this->updateImpl->update(this->actions);

    ASSERT_EQ(this->config.storedValue.size(), 2u);
    EXPECT_EQ(this->config.storedValue[0], "CLOSED");
    EXPECT_EQ(this->config.storedValue[1], "5");
}

TEST_F(CoverUpdateTest, UpdateFiresOpenState) {
    this->position = 10;
    this->closedPosition = 10;
    // Movement returns new position 50 (> closedPosition 10)
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(10);  // Same as old position, not conflicting
    this->init();

    this->updateImpl->update(this->actions);

    ASSERT_EQ(this->config.storedValue.size(), 2u);
    EXPECT_EQ(this->config.storedValue[0], "OPEN");
    EXPECT_EQ(this->config.storedValue[1], "50");
}

TEST_F(CoverUpdateTest, UpdateOmitsPositionWhenNoPosition) {
    this->position = 0;
    this->up.setUpdateReturn(10);
    this->down.setUpdateReturn(20);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->position, -1);
    ASSERT_EQ(this->config.storedValue.size(), 1u);
    // Only state name, no position value
    EXPECT_EQ(this->config.storedValue[0], "CLOSED");
}

TEST_F(CoverUpdateTest, UpdateDoesNotFireWhenUnchanged) {
    this->position = 50;
    this->previousMovementDirection = 0;
    this->stateChanged = false;
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(50);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_TRUE(this->config.storedValue.empty());
    EXPECT_EQ(this->ctx->position, 50);
    EXPECT_FALSE(this->ctx->stateChanged);
}

// ===== 5. Stopper handling =====

TEST_F(CoverUpdateTest, UpdateResetsStopperWhenStopped) {
    this->init();
    // Manually trigger the stopper (after init so reset in TearDown doesn't
    // interfere with init, but we trigger before the update call)
    this->stopper.stop();
    EXPECT_TRUE(this->stopper.isTriggered());

    // Neither moving → stopper.reset() will be called during update
    this->updateImpl->update(this->actions);

    EXPECT_FALSE(this->stopper.isTriggered());
}

TEST_F(CoverUpdateTest, UpdateKeepsStopperActive) {
    this->up.setMoving(true);
    this->up.setUpdateReturn(10);
    this->down.setUpdateReturn(0);
    this->init();
    this->stopper.stop();
    EXPECT_TRUE(this->stopper.isTriggered());

    this->updateImpl->update(this->actions);

    // Still moving, so stopper should remain triggered
    EXPECT_TRUE(this->stopper.isTriggered());
}

// ===== 6. Target position handling =====

TEST_F(CoverUpdateTest, UpdateClearsTargetWhenReached) {
    this->targetPosition = 50;
    this->position = 50;
    this->previousMovementDirection = 1;
    // Keep moving so stopper reset logic doesn't interact
    this->up.setMoving(true);
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(50);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->targetPosition, -1);
    EXPECT_EQ(this->ctx->restartCount, 0u);
}

TEST_F(CoverUpdateTest, UpdateRestartsWhenBothMovementsIdle) {
    this->targetPosition = 75;
    this->position = 50;
    this->previousMovementDirection = 0;
    this->positionSensors.clear();
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(50);
    this->init();

    // targetPosition(75) > position(50) → up.start(), down.stop()
    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->restartCount, 1u);
    EXPECT_EQ(this->up.startCount(), 1);
    EXPECT_EQ(this->down.stopCount(), 1);
    EXPECT_EQ(this->up.stopCount(), 0);
    EXPECT_EQ(this->down.startCount(), 0);
}

TEST_F(CoverUpdateTest, UpdateStopsRestartingAfterMaxAttempts) {
    this->targetPosition = 75;
    this->position = 50;
    this->restartCount = 3;
    this->previousMovementDirection = 0;
    this->positionSensors.clear();
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(50);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->targetPosition, -1);
    EXPECT_EQ(this->ctx->restartCount, 0u);
    EXPECT_EQ(this->up.stopCount(), 1);
    EXPECT_EQ(this->down.stopCount(), 1);
}

TEST_F(CoverUpdateTest, UpdateSkipsRestartWithPositionSensors) {
    this->targetPosition = 75;
    this->position = 50;
    this->previousMovementDirection = 0;
    this->positionSensors.push_back({50, 5, false});
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(50);
    this->init();

    this->updateImpl->update(this->actions);

    // Has position sensors and position != 0/100 → Action::Reset
    EXPECT_EQ(this->ctx->targetPosition, -1);
    EXPECT_EQ(this->ctx->restartCount, 0u);
    EXPECT_EQ(this->up.stopCount(), 1);
    EXPECT_EQ(this->down.stopCount(), 1);
}

TEST_F(CoverUpdateTest, UpdateIncrementsRestartCount) {
    this->targetPosition = 75;
    this->position = 50;
    this->previousMovementDirection = 0;
    this->positionSensors.clear();
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(50);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->restartCount, 1u);
}

TEST_F(CoverUpdateTest, UpdateAllowsRestartWithSensorsAtBounds) {
    // With position sensors but position is 0 (closed boundary), restart is
    // allowed because position is 0 (known position).
    this->targetPosition = 75;
    this->position = 0;
    this->previousMovementDirection = 0;
    this->positionSensors.push_back({0, 5, false});
    this->up.setUpdateReturn(0);
    this->down.setUpdateReturn(0);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->restartCount, 1u);
    EXPECT_EQ(this->up.startCount(), 1);
    EXPECT_EQ(this->ctx->targetPosition, 75);  // Not cleared
}

TEST_F(CoverUpdateTest, UpdateAllowsRestartWithSensorsAtOpenBoundary) {
    // With position sensors but position is 100 (open boundary), restart is
    // allowed because position is 100 (known position).
    this->targetPosition = 50;
    this->position = 100;
    this->previousMovementDirection = 0;
    this->positionSensors.push_back({100, 5, false});
    this->up.setUpdateReturn(100);
    this->down.setUpdateReturn(100);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->ctx->restartCount, 1u);
    EXPECT_EQ(this->down.startCount(), 1);
    EXPECT_EQ(this->ctx->targetPosition, 50);  // Not cleared
}

TEST_F(CoverUpdateTest, UpdateRestartsInCorrectDirectionDown) {
    // targetPosition < position → should start down, stop up
    this->targetPosition = 25;
    this->position = 50;
    this->previousMovementDirection = 0;
    this->positionSensors.clear();
    this->up.setUpdateReturn(50);
    this->down.setUpdateReturn(50);
    this->init();

    this->updateImpl->update(this->actions);

    EXPECT_EQ(this->down.startCount(), 1);
    EXPECT_EQ(this->up.stopCount(), 1);
    EXPECT_EQ(this->up.startCount(), 0);
    EXPECT_EQ(this->down.stopCount(), 0);
}

// ===== 7. RTC persistence =====

TEST_F(CoverUpdateTest, UpdatePersistsPositionToRtc) {
    this->position = 0;
    this->up.setUpdateReturn(42);
    this->down.setUpdateReturn(0);
    this->init();

    this->updateImpl->update(this->actions);

    // rtc.set(positionId, position + 1)
    EXPECT_EQ(this->rtc.get(this->positionId), 43u);
}
