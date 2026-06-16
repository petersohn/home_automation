# Cover Stop Debounce Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a debounce on cover stop detection that is symmetric to the existing start debounce in `CoverMovementImpl::handleDebounceAndEndPosition`. Reuse the existing `debounceTime` (20ms) constant. During the debounce window, `interpolatePosition` continues to advance the reported position (cover treated as still moving); after the debounce, `handleEndOfMovement` fires normally. `calculateMoveTimeIfNeeded` uses `stopStartTime` (not `esp.millis()`) so the calibrated travel time is not inflated by the debounce.

**Architecture:** Add one new field `stopStartTime` to `CoverMovementImpl`. Manage it in `update()` (bounce reset, set on first "not moving" while `isReallyMoving()`). Use it in `trackMovement` to gate `handleEndOfMovement` and to call `interpolatePosition` while in the debounce window. Use it in `calculateMoveTimeIfNeeded` to measure the actual stop time.

**Tech Stack:** C++17, Arduino/ESP8266 (device), GoogleTest (native tests), `clang-format`.

**Working directory:** All paths are relative to the project root. Build commands assume you're in `build/`. The build picks up `src/common/*.cpp` and `test/*.cpp` automatically (CMake `GLOB`).

**Conventions:**
- Instance members prefixed with `this->`
- `clang-format` on all modified files
- `git commit` after each task
- Use `arduino-cli compile --fqbn esp8266:esp8266:generic --verify` for device build
- Use `./build/home_automation_test` for native tests

**Spec:** `docs/superpowers/specs/2026-06-15-cover-stop-debounce-design.md`

---

## File structure

| File | Role |
|------|------|
| `src/common/CoverMovementImpl.hpp` | Add `stopStartTime` field |
| `src/common/CoverMovementImpl.cpp` | Debounce logic in `update()`, `trackMovement`; use `stopStartTime` in `calculateMoveTimeIfNeeded` |
| `test/CoverMovementTest.cpp` | Add `UpdateMovementStopDebounce`, `UpdateMovementStopDebounceBounce`; add `advanceMs(20)` to 3 existing tests |
| `test/CoverTest.cpp` | Adjust `delay=10` and `delay >= 50` iterations to account for the 1-round debounce delay |

No new files. No changes to `Cover`, `CoverState`, `CoverStop`, `CoverUpdate`, or `CoverMovement` (abstract base).

---

## Task 1: Add `stopStartTime` field to `CoverMovementImpl`

**Files:**
- Modify: `src/common/CoverMovementImpl.hpp:103-106`

- [ ] **Step 1: Add the field**

Open `src/common/CoverMovementImpl.hpp`. Find the existing state field block (around line 103-106):

```cpp
    unsigned long moveStartTime = 0;
    unsigned long startedTime = 0;
    int moveStartPosition = -2;
    bool startTriggered = false;
```

Add a new field for the stop debounce. The new block should be:

```cpp
    unsigned long moveStartTime = 0;
    unsigned long startedTime = 0;
    unsigned long stopStartTime = 0;
    int moveStartPosition = -2;
    bool startTriggered = false;
```

- [ ] **Step 2: Format and build**

```bash
clang-format -i src/common/CoverMovementImpl.hpp
cd build && make home_automation_test -j$(nproc)
```

Expected: build succeeds, no behavior change (existing tests still pass).

- [ ] **Step 3: Verify device build**

```bash
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
```

Expected: device build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/common/CoverMovementImpl.hpp
git commit -m "CoverMovementImpl: add stopStartTime field for stop debounce"
```

---

## Task 2: Write the failing `UpdateMovementStopDebounce` test

**Files:**
- Modify: `test/CoverMovementTest.cpp` (add test at end, before `}  // namespace`)

- [ ] **Step 1: Add the new test**

Open `test/CoverMovementTest.cpp`. Find the line `}  // namespace` at the end of the file (around line 590). Add a new test before it:

```cpp
// ============= update() — stop debounce =============

TEST_F(CoverMovementTest, UpdateMovementStopDebounce) {
    // Pre-set RTC so moveTime is known and interpolatePosition is active
    this->rtc.set(0, 1000);

    CoverStopImpl stopper(this->esp, this->state, this->stopPin, false);
    CoverMovementImpl movement(
        this->state, stopper, this->inputPin, this->outputPin,
        this->endPositionUp, this->upDirection, "Up");

    this->advanceMs(1);
    movement.start();
    this->esp.digitalWrite(this->inputPin, 1);

    // Get the cover "really moving" past the start debounce
    this->advanceMs(5);
    movement.update();   // first update sets moveStartTime
    this->advanceMs(20);
    movement.update();   // past start debounce, moveStartPosition = 0

    // Motor stops
    this->esp.digitalWrite(this->inputPin, 0);

    // Immediately after stop: still in stop debounce, position interpolated
    // as if still moving (now = moveStartTime + 26, position advances)
    this->debug.str("");
    int pos = movement.update();
    EXPECT_EQ(pos, 1);                  // beginPosition + direction
    EXPECT_TRUE(movement.isStarted());  // not stopped yet

    // Advance past debounce and update: end of movement fires
    this->advanceMs(20);
    this->debug.str("");
    pos = movement.update();
    EXPECT_EQ(pos, this->endPositionUp);
    EXPECT_FALSE(movement.isStarted());
    this->expectLogContains("End position reached.");
}
```

- [ ] **Step 2: Build and run the new test (expect FAIL)**

```bash
cd build && make home_automation_test -j$(nproc)
./build/home_automation_test --gtest_filter='CoverMovementTest.UpdateMovementStopDebounce'
```

Expected: FAIL. The cover will hit `handleEndOfMovement` on the first stop (no debounce yet) so `pos == endPositionUp` already at the first assertion, not `1`. The log will contain "End position reached." prematurely.

- [ ] **Step 3: Commit the failing test**

```bash
git add test/CoverMovementTest.cpp
git commit -m "test: add failing UpdateMovementStopDebounce"
```

---

## Task 3: Implement the stop debounce in `update()` and `trackMovement`

**Files:**
- Modify: `src/common/CoverMovementImpl.cpp:101-131` (`update()`)
- Modify: `src/common/CoverMovementImpl.cpp:214-228` (`trackMovement`)
- Modify: `src/common/CoverMovementImpl.cpp:266-272` (`resetStateIfStopped`)

- [ ] **Step 1: Update `update()` to manage `stopStartTime` and gate `resetStateIfStopped`**

Open `src/common/CoverMovementImpl.cpp`. Replace the entire `update()` function (lines 101-131) with:

```cpp
int CoverMovementImpl::update() {
    const auto now = this->state.esp.millis();
    const bool moving = this->isMoving();
    int newPosition = this->state.position;

    this->resetLatchingStartIfMoving(moving);

    // Stop debounce: record when "really moving" first transitioned to
    // "not moving"; clear it on bounce (moving went back to true).
    if (moving) {
        this->stopStartTime = 0;
    } else if (this->isReallyMoving() && this->stopStartTime == 0) {
        this->stopStartTime = now;
    }

    const bool hasActivePositionSensor = this->state.activePositionSensor >= 0;
    if (hasActivePositionSensor) {
        this->updateWithActivePositionSensor();
    } else {
        newPosition =
            this->updateWithoutActivePositionSensor(moving, now, newPosition);
    }

    if (isReallyMoving()) {
        newPosition = this->trackMovement(
            hasActivePositionSensor, moving, now, newPosition);
    }

    if (!this->isReallyMoving() && !moving && this->isStarted() &&
        now - this->startedTime > startTimeout) {
        newPosition = this->checkStartTimeout(newPosition);
    }

    // State reset is also debounced: if we just observed "not moving" and
    // the debounce window hasn't elapsed, the cover is still considered
    // "really moving" (moveStartPosition kept), so the checkStartTimeout
    // branch above does not fire on the next call with stale startedTime.
    if (!moving && this->stopStartTime != 0 &&
        now - this->stopStartTime >= debounceTime) {
        this->resetStateIfStopped();
    }

    return newPosition;
}
```

- [ ] **Step 2: Update `trackMovement` to gate `handleEndOfMovement` and add interpolation during debounce**

Replace the entire `trackMovement` function (lines 214-228) with:

```cpp
int CoverMovementImpl::trackMovement(
    bool hasActivePositionSensor, bool moving, unsigned long now,
    int newPosition) {
    if (moving) {
        if (!hasActivePositionSensor && this->moveTimeIndex >= 0) {
            return this->interpolatePosition(now, newPosition);
        } else {
            return newPosition;
        }
    }
    if (this->isStarted()) {
        if (this->stopStartTime != 0 &&
            now - this->stopStartTime >= debounceTime) {
            newPosition = this->handleEndOfMovement(newPosition);
        } else if (!hasActivePositionSensor && this->moveTimeIndex >= 0) {
            // Continue interpolating during the stop debounce window,
            // as if the cover were still moving.
            newPosition = this->interpolatePosition(now, newPosition);
        }
    }
    return newPosition;
}
```

- [ ] **Step 3: Update `resetStateIfStopped` to clear `stopStartTime`**

Replace the `resetStateIfStopped` function (lines 266-272) with:

```cpp
void CoverMovementImpl::resetStateIfStopped() {
    if (this->isReallyMoving()) {
        this->log("Stopped moving");
    }
    this->moveStartTime = 0;
    this->moveStartPosition = mspNotMoving;
    this->stopStartTime = 0;
}
```

- [ ] **Step 4: Format and run the new test (expect PASS)**

```bash
clang-format -i src/common/CoverMovementImpl.cpp
cd build && make home_automation_test -j$(nproc)
./build/home_automation_test --gtest_filter='CoverMovementTest.UpdateMovementStopDebounce'
```

Expected: PASS.

- [ ] **Step 5: Run the full CoverMovementTest suite**

```bash
./build/home_automation_test --gtest_filter='CoverMovementTest.*'
```

Expected: 3 existing tests fail: `UpdateMovementEndReached`, `UpdateDownDirection`, `CalculateMoveTimeSavesToRtc`. They all set the input pin low and call `update()` immediately, which now lands in the debounce window.

- [ ] **Step 6: Commit**

```bash
git add src/common/CoverMovementImpl.cpp
git commit -m "CoverMovementImpl: implement stop debounce symmetric to start"
```

---

## Task 4: Fix the 3 existing tests that land in the debounce window

**Files:**
- Modify: `test/CoverMovementTest.cpp` (3 tests)

- [ ] **Step 1: Fix `UpdateMovementEndReached`**

In `test/CoverMovementTest.cpp`, find `UpdateMovementEndReached` (around line 319). The test ends with:

```cpp
    // Motor reaches end stop and stops
    this->esp.digitalWrite(this->inputPin, 0);

    this->debug.str("");
    int pos = movement.update();

    // End reached: position should be endPosition
    EXPECT_EQ(pos, this->endPositionUp);
    EXPECT_EQ(this->esp.digitalRead(this->outputPin), 0);
    EXPECT_FALSE(movement.isStarted());
    this->expectLogContains("End position reached.");
```

Add an `advanceMs(20)` after setting the input pin low and before the `update()` call. Replace the snippet with:

```cpp
    // Motor reaches end stop and stops
    this->esp.digitalWrite(this->inputPin, 0);
    this->advanceMs(20);

    this->debug.str("");
    int pos = movement.update();

    // End reached: position should be endPosition
    EXPECT_EQ(pos, this->endPositionUp);
    EXPECT_EQ(this->esp.digitalRead(this->outputPin), 0);
    EXPECT_FALSE(movement.isStarted());
    this->expectLogContains("End position reached.");
```

- [ ] **Step 2: Fix `UpdateDownDirection`**

Find `UpdateDownDirection` (around line 355). The test has a similar stop block:

```cpp
    // Motor stops
    this->esp.digitalWrite(this->inputPin, 0);
    this->debug.str("");
    pos = movement.update();

    EXPECT_EQ(pos, this->endPositionDown);
    EXPECT_EQ(this->esp.digitalRead(this->outputPin), 0);
    EXPECT_FALSE(movement.isStarted());
```

Add an `advanceMs(20)` between the stop and the `update()`. Replace with:

```cpp
    // Motor stops
    this->esp.digitalWrite(this->inputPin, 0);
    this->advanceMs(20);
    this->debug.str("");
    pos = movement.update();

    EXPECT_EQ(pos, this->endPositionDown);
    EXPECT_EQ(this->esp.digitalRead(this->outputPin), 0);
    EXPECT_FALSE(movement.isStarted());
```

- [ ] **Step 3: Fix `CalculateMoveTimeSavesToRtc`**

Find `CalculateMoveTimeSavesToRtc` (around line 551). The test has:

```cpp
    // Motor stops at end
    this->esp.digitalWrite(this->inputPin, 0);
    this->debug.str("");
    movement.update();

    // calculateMoveTimeIfNeeded should have called rtc.set() for id 0
    // The move time should now be non-zero (~325ms)
    auto moveTime = this->rtc.get(0);
    EXPECT_GT(moveTime, 0u);
    this->expectLogContains("Move time:");
```

Add an `advanceMs(20)` between the stop and the `update()`. Replace with:

```cpp
    // Motor stops at end
    this->esp.digitalWrite(this->inputPin, 0);
    this->advanceMs(20);
    this->debug.str("");
    movement.update();

    // calculateMoveTimeIfNeeded should have called rtc.set() for id 0
    // The move time should now be non-zero (~300ms, not 325ms —
    // see Task 5 for the calculateMoveTimeIfNeeded change)
    auto moveTime = this->rtc.get(0);
    EXPECT_GT(moveTime, 0u);
    this->expectLogContains("Move time:");
```

- [ ] **Step 4: Run the full CoverMovementTest suite (expect PASS)**

```bash
cd build && make home_automation_test -j$(nproc)
./build/home_automation_test --gtest_filter='CoverMovementTest.*'
```

Expected: all `CoverMovementTest.*` tests pass.

- [ ] **Step 5: Commit**

```bash
git add test/CoverMovementTest.cpp
git commit -m "test: add advanceMs(20) after motor stops in 3 existing tests"
```

---

## Task 5: Update `calculateMoveTimeIfNeeded` to use `stopStartTime`

**Files:**
- Modify: `src/common/CoverMovementImpl.cpp:292-303` (`calculateMoveTimeIfNeeded`)

- [ ] **Step 1: Update `calculateMoveTimeIfNeeded`**

Open `src/common/CoverMovementImpl.cpp`. Replace `calculateMoveTimeIfNeeded` (lines 292-303) with:

```cpp
void CoverMovementImpl::calculateMoveTimeIfNeeded() {
    if (this->moveTimeIndex < 0) {
        return;
    }

    auto& moveTime = this->moveTimes[this->moveTimeIndex];
    if (this->moveStartPosition == this->beginPosition) {
        // Use stopStartTime (not esp.millis()) so the stop debounce
        // does not inflate the measured travel time.
        moveTime.time = this->stopStartTime - this->moveStartTime;
        this->state.rtc.set(moveTime.rtcId, moveTime.time);
        this->log("Move time: " + tools::intToString(moveTime.time));
    }
}
```

- [ ] **Step 2: Build and run all CoverMovementTest (expect PASS)**

```bash
clang-format -i src/common/CoverMovementImpl.cpp
cd build && make home_automation_test -j$(nproc)
./build/home_automation_test --gtest_filter='CoverMovementTest.*'
```

Expected: all tests pass. The `CalculateMoveTimeSavesToRtc` test expects `moveTime > 0`, which is still true; the actual value is now ~300ms instead of ~325ms.

- [ ] **Step 3: Verify device build**

```bash
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
```

Expected: device build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/common/CoverMovementImpl.cpp
git commit -m "CoverMovementImpl: use stopStartTime in calculateMoveTimeIfNeeded"
```

---

## Task 6: Write the failing `UpdateMovementStopDebounceBounce` test

**Files:**
- Modify: `test/CoverMovementTest.cpp` (add test at end, before `}  // namespace`)

- [ ] **Step 1: Add the new test**

Open `test/CoverMovementTest.cpp`. Find the `UpdateMovementStopDebounce` test (just added in Task 2) and add a sibling test after it. Insert the following just before `}  // namespace`:

```cpp
TEST_F(CoverMovementTest, UpdateMovementStopDebounceBounce) {
    // Pre-set RTC so moveTime is known and interpolatePosition is active
    this->rtc.set(0, 1000);

    CoverStopImpl stopper(this->esp, this->state, this->stopPin, false);
    CoverMovementImpl movement(
        this->state, stopper, this->inputPin, this->outputPin,
        this->endPositionUp, this->upDirection, "Up");

    this->advanceMs(1);
    movement.start();
    this->esp.digitalWrite(this->inputPin, 1);

    // Get past the start debounce
    this->advanceMs(5);
    movement.update();
    this->advanceMs(20);
    movement.update();  // past start debounce, moveStartPosition = 0

    // Move for 500ms so we have a non-trivial interpolated position
    this->advanceMs(500);
    this->debug.str("");
    int pos = movement.update();
    // position = 0 + 100 * 500 / 1000 = 50
    EXPECT_EQ(pos, 50);
    EXPECT_TRUE(movement.isStarted());

    // Brief stop blip (10ms < 20ms debounce). During the debounce window,
    // interpolation should continue as if the cover were still moving.
    this->esp.digitalWrite(this->inputPin, 0);
    this->advanceMs(10);
    this->debug.str("");
    pos = movement.update();
    // Interpolation continued: position advanced by 10ms of "motion"
    // = 0 + 100 * (500+10) / 1000 = 51
    EXPECT_EQ(pos, 51);
    EXPECT_TRUE(movement.isStarted());

    // Bounce back to moving within debounce window: bounce reset
    this->esp.digitalWrite(this->inputPin, 1);
    this->advanceMs(5);
    this->debug.str("");
    pos = movement.update();
    // Still interpolating as if still moving: position advances
    // = 0 + 100 * (510+5) / 1000 = 51 (rounded down from 51.5)
    EXPECT_EQ(pos, 51);
    EXPECT_TRUE(movement.isStarted());

    // Now stop for real, well past total debounce
    this->esp.digitalWrite(this->inputPin, 0);
    this->advanceMs(20);
    this->debug.str("");
    pos = movement.update();

    // End of movement fires: position is endPosition, movement stopped
    EXPECT_EQ(pos, this->endPositionUp);
    EXPECT_FALSE(movement.isStarted());
    this->expectLogContains("End position reached.");
}
```

- [ ] **Step 2: Run the new test (expect PASS)**

```bash
cd build && make home_automation_test -j$(nproc)
./build/home_automation_test --gtest_filter='CoverMovementTest.UpdateMovementStopDebounceBounce'
```

Expected: PASS — the debounce implementation from Task 3 already handles bouncing correctly.

- [ ] **Step 3: Commit**

```bash
git add test/CoverMovementTest.cpp
git commit -m "test: add UpdateMovementStopDebounceBounce"
```

---

## Task 7: Update CoverTest.cpp integration tests for the 1-round debounce delay

The new debounce adds at least one full `update()` round between "stop detected" and `handleEndOfMovement` firing. For `delay=10` it adds two rounds; for `delay >= 50` it adds one round. Fix each affected test by either extending the post-stop wait or skipping the first iteration's check.

**Files:**
- Modify: `test/CoverTest.cpp` (6 test families)

- [ ] **Step 1: Fix `BasicFixture.Open` (line ~351)**

Find the test and locate the post-open block. The current block is:

```cpp
        } else {
            EXPECT_TRUE(!this->isMovingUp());
            EXPECT_EQ(this->getValue(0), "OPEN");
            EXPECT_EQ(this->getValue(1), "100");
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(10100, delay, func));
}
```

With the debounce, `isMovingUp()` is true for one extra iteration after reaching 100. The cleanest fix is to advance time by 20ms before the loop starts, or to use a helper loop that skips the first post-open iteration. Simpler: add a 20ms `advanceMs` before `loopFor`:

```cpp
        } else {
            EXPECT_TRUE(!this->isMovingUp());
            EXPECT_EQ(this->getValue(0), "OPEN");
            EXPECT_EQ(this->getValue(1), "100");
        }
    };
    this->esp.delay(20);
    ASSERT_NO_FATAL_FAILURE(this->loopFor(10100, delay, func));
}
```

- [ ] **Step 2: Fix `BasicFixture.Close` (line ~428)**

Find the test and locate the post-close block. Apply the same `this->esp.delay(20);` before the `loopFor(10100, ...)` call.

- [ ] **Step 3: Fix `BasicFixture.StopWhileOpening` / `StopWhileClosing` (lines ~460, ~482)**

For each, the post-stop block is:

```cpp
    this->stop();
    this->esp.delay(delay);
    this->loop();

    EXPECT_FALSE(this->isMovingUp());
    EXPECT_FALSE(this->isMovingDown());
    EXPECT_EQ(this->position, 2000);
}
```

Add a second `loop()` call (or `advanceMs(20)`) before the assertions. Replace with:

```cpp
    this->stop();
    this->esp.delay(delay);
    this->loop();
    this->esp.delay(20);
    this->loop();

    EXPECT_FALSE(this->isMovingUp());
    EXPECT_FALSE(this->isMovingDown());
    EXPECT_EQ(this->position, 2000);
}
```

Apply to both `StopWhileOpening` and `StopWhileClosing`.

- [ ] **Step 4: Fix `BasicFixture.StopEarlyWhileCalibrating` (line ~978)**

Find the test and locate the post-stop block. Current:

```cpp
    this->stop();
    this->esp.delay(delay);
    this->loop();

    auto checkNotMoving = [&](unsigned long, size_t) {
        EXPECT_FALSE(this->isMovingUp());
        EXPECT_FALSE(this->isMovingDown());
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(delay * 3, delay, checkNotMoving));
```

Add `this->esp.delay(20);` between the single `loop()` and the `loopFor` to clear the debounce before checking:

```cpp
    this->stop();
    this->esp.delay(delay);
    this->loop();
    this->esp.delay(20);

    auto checkNotMoving = [&](unsigned long, size_t) {
        EXPECT_FALSE(this->isMovingUp());
        EXPECT_FALSE(this->isMovingDown());
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(delay * 3, delay, checkNotMoving));
```

- [ ] **Step 5: Fix `StopMomentarilyWhileCalibratingFixture.StopMomentarilyWhileCalibrating` (line ~1010)**

Find the test and locate the post-stop block. Current:

```cpp
    this->movingUp = false;
    this->loop();

    auto checkNotMoving = [&](unsigned long, size_t) {
        EXPECT_FALSE(this->isMovingUp());
        EXPECT_FALSE(this->isMovingDown());
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(delay * 3, delay, checkNotMoving));
```

Add `this->esp.delay(20);` between the `loop()` and the `loopFor`:

```cpp
    this->movingUp = false;
    this->loop();
    this->esp.delay(20);

    auto checkNotMoving = [&](unsigned long, size_t) {
        EXPECT_FALSE(this->isMovingUp());
        EXPECT_FALSE(this->isMovingDown());
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(delay * 3, delay, checkNotMoving));
```

- [ ] **Step 6: Fix `CalibrateFixture.Calibrate` (line ~505)**

The test has several `loopFor(delay, delay, ...)` blocks at direction changeovers. With the debounce, the first iteration of each lands in the debounce window and the assertions there will fail. Extend each of these brief blocks from `loopFor(delay, delay, ...)` to `loopFor(delay * 3, delay, ...)`.

There are 3 such blocks in this test:
- After opening: `ASSERT_NO_FATAL_FAILURE(this->loopFor(delay, delay, funcOpen));` → `loopFor(delay * 3, delay, funcOpen)`
- After closing: `ASSERT_NO_FATAL_FAILURE(this->loopFor(delay, delay, [&](...)));` (with `isMovingUp()` and `getValue(0) == "CLOSED"`) → `loopFor(delay * 3, delay, [&](...))`
- After opening in phase 3: `ASSERT_NO_FATAL_FAILURE(this->loopFor(delay, delay, [&](...)));` (with `isMovingDown()` and `getValue(0) == "OPEN"`) → `loopFor(delay * 3, delay, [&](...))`

Also extend the final assertion block in the `hasPositionSensor && start == 0` branch:

```cpp
ASSERT_NO_FATAL_FAILURE(
    this->loopFor(delay, delay, [&](unsigned long, size_t) {
    EXPECT_FALSE(this->isMovingUp());
    EXPECT_FALSE(this->isMovingDown());
    EXPECT_EQ(this->getValue(0), "OPENING");
    EXPECT_EQ(this->getValue(1), "40");
}));
```

→ `loopFor(delay * 3, delay, ...)`.

- [ ] **Step 7: Run the full CoverTest suite (expect PASS)**

```bash
cd build && make home_automation_test -j$(nproc)
./build/home_automation_test --gtest_filter='CoverTest*.*'
```

Expected: all `CoverTest*` tests pass (or skip where the fixture already skips).

- [ ] **Step 8: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test: account for 1-round stop debounce in CoverTest integration tests"
```

---

## Task 8: Final verification

- [ ] **Step 1: Run the full native test suite**

```bash
cd build && make home_automation_test -j$(nproc)
./build/home_automation_test
```

Expected: all tests pass (no skips beyond the pre-existing `hasPositionSensor && delay == 500` skips in `Calibrate`).

- [ ] **Step 2: Verify the device build**

```bash
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
```

Expected: device build succeeds.

- [ ] **Step 3: Format all modified files**

```bash
clang-format -i src/common/CoverMovementImpl.hpp src/common/CoverMovementImpl.cpp test/CoverMovementTest.cpp test/CoverTest.cpp
```

- [ ] **Step 4: Verify no uncommitted changes remain**

```bash
git status
```

Expected: working tree clean (everything committed by tasks above).

- [ ] **Step 5: Tag the spec/plan pair**

```bash
git log --oneline docs/superpowers/specs/2026-06-15-cover-stop-debounce-design.md docs/superpowers/plans/2026-06-15-cover-stop-debounce.md
```

Expected: lists the design and plan commits.
