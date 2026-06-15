# Cover Stop Debounce: Symmetric to Start Debounce

## Goal

Add a debounce on cover stop detection, symmetric to the existing start debounce in `CoverMovementImpl::handleDebounceAndEndPosition`. The same `debounceTime` constant (20ms) is reused. Behavior during the debounce window: the position is held at its last value (no jump to `endPosition`, no `handleEndOfMovement` side effects). After the debounce elapses, end-of-movement logic runs as it does today.

This protects against noise/bounce on the movement input pin: a brief `moving=false` blip while the cover is still in motion should not be treated as a real stop.

---

## Background: the start debounce

`src/common/CoverMovementImpl.cpp:198-212` already debounces "started moving":

```cpp
int CoverMovementImpl::handleDebounceAndEndPosition(
    unsigned long now, int newPosition) {
    if (this->moveStartTime == 0) {
        this->moveStartTime = now;
    } else if (
        !this->isReallyMoving() && now - this->moveStartTime >= debounceTime) {
        this->moveStartPosition = this->state.position;
        this->log("Started moving");
    }
    ...
}
```

- `moveStartTime` records the first time the input pin reports `moving=true`.
- The cover is marked "really moving" (`moveStartPosition` set) only after `debounceTime` of stable `moving=true`.
- During the debounce window, `moveStartPosition` is still `mspNotMoving`, so `isReallyMoving()` is false, so `trackMovement` is not called, and the position reported to the caller is just `state.position` (the unchanged value carried in from `update()`). No position interpolation, no end-of-movement side effects.

The symmetric stop debounce needs an analogous gate on the `!moving` -> end-of-movement path.

---

## Approach: new `stopStartTime` field, gate inside `update()` and `trackMovement`

Add a single new field `unsigned long stopStartTime = 0` to `CoverMovementImpl`. The debounce is applied in two places, both inside `update()`:

1. `trackMovement` only calls `handleEndOfMovement` after the debounce elapses (gates the position/state change to "stopped").
2. The `resetStateIfStopped()` call at the end of `update()` is also gated by the debounce (otherwise the cover is treated as "really stopped" on the very first call where `moving=false`, which would leave `startedTime` set and could trigger a false-positive `checkStartTimeout` on the next call).

The field is set once when `!moving` is first observed while `isReallyMoving()` is true. It is reset to 0 in two places at runtime: when `moving` becomes true (bounce reset), and inside `resetStateIfStopped()` (after the debounce elapses). The default value of 0 comes from the field's in-class initializer in the header.

This is a direct mirror of the start debounce structure: `moveStartTime` / `moveStartPosition` for start, `stopStartTime` for stop. No reuse of `moveStartTime` for both — the two debounces are independent state machines and should remain independent.

---

## File layout

### Modified files
| File | Change |
|------|--------|
| `src/common/CoverMovementImpl.hpp` | Add `unsigned long stopStartTime = 0;` member |
| `src/common/CoverMovementImpl.cpp` | Gate `handleEndOfMovement` in `trackMovement`; reset `stopStartTime` in `update()` and `resetStateIfStopped()` |
| `test/CoverMovementTest.cpp` | Add `advanceMs(20)` after the motor stops in three existing tests (`UpdateMovementEndReached`, `UpdateDownDirection`, `CalculateMoveTimeSavesToRtc`); add a new test that verifies the stop debounce |
| `test/CoverTest.cpp` | Update the `delay=10` parameterized runs of `BasicFixture.Open`, `BasicFixture.Close`, `BasicFixture.StopWhileOpening`, `BasicFixture.StopWhileClosing`, `BasicFixture.StopEarlyWhileCalibrating`, `StopMomentarilyWhileCalibratingFixture.StopMomentarilyWhileCalibrating`, and `CalibrateFixture.Calibrate` to account for the 20ms debounce. See "Integration test impact" below. |

### Unchanged files
- `src/common/Cover.hpp` / `.cpp`
- `src/common/CoverState.hpp`
- `src/common/CoverStop.hpp` / `CoverStopImpl.hpp` / `CoverStopImpl.cpp`
- `src/common/CoverUpdate.hpp` / `.cpp`
- `src/common/CoverMovement.hpp`

The CoverStop and CoverUpdate classes do not need changes. The latching vs continuous mode behavior in `handleStopped` is unaffected — the debounce only delays when `handleStopped` is called.

### Integration test impact (`test/CoverTest.cpp`)

The integration test in `test/CoverTest.cpp` simulates a real cover by setting the input pins (`UpInput`/`DownInput`) based on simulated motion and calling `updateInterface()`. Loop delays are `delays1[] = {10, 50, 100, 500}` and `delays2[] = {10, 50, 100}` ms.

With the new debounce, the cover's `handleEndOfMovement` (and therefore the output-pin deactivation) is delayed by `debounceTime` (20ms) after the simulated motion stops. This affects only `delay=10` runs (the others are >= 20ms and clear the debounce in a single loop iteration). For `delay=10`, the cover's output stays HIGH for one extra loop iteration after reaching the end position, and the cover cannot start a new direction for that extra iteration either (because `startedTime` is not yet reset).

Concretely, for `delay=10` the following test families need either:
1. A one-iteration buffer (advance time by at least 20ms after the cover reaches the end), or
2. A loosened expectation in the iteration that lands in the debounce window.

Affected parameterized tests (all under `CoverTest.cpp`):
- `BasicFixture.Open` (line ~351) — at `time == 10000` the cover should be fully open. With debounce, `isMovingUp()` is still true at the first `delay=10` iteration after reaching 100. Fix: allow `isMovingUp()` to be true for one extra iteration (or skip the first post-end assertion).
- `BasicFixture.Close` (line ~428) — symmetric: at `time == 10000` the cover should be fully closed. Same fix.
- `BasicFixture.StopWhileOpening` / `StopWhileClosing` (lines ~460, ~482) — the post-stop `loop()` call is one iteration at `delay=10`. The test checks `isMovingUp()/isMovingDown()` are false; with debounce, the output pin is still HIGH for 20ms after `stop()`. Fix: add a second `loop()` call (or `advanceMs(20)`) before asserting.
- `BasicFixture.StopEarlyWhileCalibrating` (line ~978) — same issue: one `loop()` after `stop()`, then `loopFor(delay * 3, delay, checkNotMoving)`. The `delay * 3 = 30ms` window covers the 20ms debounce, so the second assertion in the loop should pass. But the FIRST `checkNotMoving` iteration is at `delay=10` after the stop and will see the output still HIGH. Fix: drop the first iteration's check or start the `loopFor` after `advanceMs(20)`.
- `StopMomentarilyWhileCalibratingFixture.StopMomentarilyWhileCalibrating` (line ~1010) — `this->movingUp = false; this->loop();` then `loopFor(delay * 3, delay, checkNotMoving)`. Same pattern: the first `checkNotMoving` iteration lands in the debounce window. Fix: same as above.
- `CalibrateFixture.Calibrate` (line ~505) — uses `loopFor` to run a full open-close-open cycle. With `delay=10`, every direction changeover is delayed by 20ms. The post-direction-change `loopFor(delay, delay, ...)` blocks run for 10ms only and will land inside the debounce window. Fix: extend those brief `loopFor` blocks to `loopFor(20 + delay, delay, ...)` (or to `delay * 3` for symmetry with existing patterns).

For `delay >= 50`, the debounce fits inside a single loop iteration, so no changes are needed.

---

## Module details

### 1. `CoverMovementImpl.hpp` — new field

```cpp
unsigned long moveStartTime = 0;
unsigned long startedTime = 0;
unsigned long stopStartTime = 0;   // NEW: first time "not moving" detected while really moving
int moveStartPosition = -2;
bool startTriggered = false;
```

### 2. `CoverMovementImpl.cpp` — gate in `trackMovement` and `update()`

Current `trackMovement` (`src/common/CoverMovementImpl.cpp:214-228`):

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
        newPosition = this->handleEndOfMovement(newPosition);
    }
    return newPosition;
}
```

New `trackMovement`:

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
    if (this->isStarted() && this->stopStartTime != 0 &&
        now - this->stopStartTime >= debounceTime) {
        newPosition = this->handleEndOfMovement(newPosition);
    }
    return newPosition;
}
```

New `update()` (only the changed portions shown; unchanged lines elided):

```cpp
int CoverMovementImpl::update() {
    const auto now = this->state.esp.millis();
    const bool moving = this->isMoving();
    int newPosition = this->state.position;

    this->resetLatchingStartIfMoving(moving);

    // Stop debounce: record when "really moving" first transitioned to "not moving";
    // clear it on bounce (moving went back to true).
    if (moving) {
        this->stopStartTime = 0;
    } else if (this->isReallyMoving() && this->stopStartTime == 0) {
        this->stopStartTime = now;
    }

    const bool hasActivePositionSensor = this->state.activePositionSensor >= 0;
    // ... existing position update branches unchanged ...

    if (isReallyMoving()) {
        newPosition = this->trackMovement(
            hasActivePositionSensor, moving, now, newPosition);
    }

    if (!this->isReallyMoving() && !moving && this->isStarted() &&
        now - this->startedTime > startTimeout) {
        newPosition = this->checkStartTimeout(newPosition);
    }

    // State reset is also debounced: if we just observed "not moving" and the
    // debounce window hasn't elapsed, the cover is still considered "really
    // moving" (moveStartPosition kept), so the checkStartTimeout branch above
    // does not fire on the next call with stale startedTime.
    if (!moving && this->stopStartTime != 0 &&
        now - this->stopStartTime >= debounceTime) {
        this->resetStateIfStopped();
    }

    return newPosition;
}
```

### 3. `CoverMovementImpl.cpp` — reset in `resetStateIfStopped()`

`resetStateIfStopped` (`src/common/CoverMovementImpl.cpp:266-272`) currently resets `moveStartTime` and `moveStartPosition`. Add `stopStartTime = 0` to the same block so the debounce does not retrigger on subsequent calls.

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

### 4. Symmetry recap

| State | Start debounce | Stop debounce |
|-------|---------------|---------------|
| Field that records first detection | `moveStartTime` | `stopStartTime` |
| Threshold | `debounceTime` (20ms) | `debounceTime` (20ms) |
| Constant location | `src/common/CoverMovementImpl.cpp:7` | (same) |
| Set in | `handleDebounceAndEndPosition` (no sensors, `moving=true`) | `update()` (`!moving && isReallyMoving() && stopStartTime == 0`) |
| Reset on detection of the opposite | n/a — start debounce runs only on `moving=true` | `moving=true` resets `stopStartTime = 0` in `update()` |
| Reset in cleanup | `resetStateIfStopped` already does this for `moveStartTime` | `resetStateIfStopped` also resets `stopStartTime` |
| Position during debounce | returned value is `state.position` (unchanged) | returned value is `state.position` (unchanged) |
| Other side effects during debounce | `moveStartPosition` not set yet | `handleEndOfMovement` not called, `moveStartPosition`/`moveStartTime` not reset |

---

## Test changes

### Existing tests that simulate "motor stops"

Three tests in `test/CoverMovementTest.cpp` currently set the input pin low and then immediately call `update()`, expecting the end-of-movement behavior in the same call. They need an additional `advanceMs(20)` between the stop and the assertion:

1. `UpdateMovementEndReached` (line ~319)
2. `UpdateDownDirection` (line ~355)
3. `CalculateMoveTimeSavesToRtc` (line ~551)

### New test: `UpdateMovementStopDebounce`

Add a test that verifies the symmetric debounce:

```cpp
TEST_F(CoverMovementTest, UpdateMovementStopDebounce) {
    // Pre-set RTC so moveTime is known
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

    // Immediately after stop: still in stop debounce, position unchanged,
    // movement still considered started
    this->debug.str("");
    int pos = movement.update();
    EXPECT_EQ(pos, 0);                  // not at endPosition yet
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

### New test: `UpdateMovementStopDebounceBounce`

Verify that bouncing (stop → move within debounce) restarts the debounce, and assert position at each step. No pre-set RTC move time, so `interpolatePosition` is never active and the position stays at `state.position` (0) until `handleEndOfMovement` fires.

```cpp
TEST_F(CoverMovementTest, UpdateMovementStopDebounceBounce) {
    // No RTC pre-set: moveTime stays 0, so interpolatePosition is never
    // called and position stays at state.position (0) until
    // handleEndOfMovement fires.

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
    movement.update();
    EXPECT_TRUE(movement.isStarted());

    // Brief stop blip (10ms < 20ms debounce)
    this->esp.digitalWrite(this->inputPin, 0);
    this->advanceMs(10);
    this->debug.str("");
    int pos = movement.update();
    EXPECT_EQ(pos, 0);                  // not at endPosition yet
    EXPECT_TRUE(movement.isStarted());  // not stopped yet
    // No "Stopped moving" log: resetStateIfStopped is debounced-gated

    // Bounce back to moving within debounce window
    this->esp.digitalWrite(this->inputPin, 1);
    this->advanceMs(5);
    this->debug.str("");
    pos = movement.update();
    EXPECT_EQ(pos, 0);                  // still not at endPosition
    EXPECT_TRUE(movement.isStarted());  // still not stopped

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

---

## Out of scope

- Changing `debounceTime` to a configurable value (still 20ms hard-coded).
- Changing the latching vs continuous mode behavior in `handleStopped`.
- Debouncing the start timeout path in `checkStartTimeout` — the cover never moved in that case, so the start debounce semantics don't apply.
- Changes to `CoverStop` or `CoverUpdate`.
- Changes to the abstract `CoverMovement` base.
