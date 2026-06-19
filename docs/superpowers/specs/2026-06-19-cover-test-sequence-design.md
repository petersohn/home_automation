# CoverTest Sequence-Based Assertions

## Goal

Replace the per-iteration timing assertions in `test/CoverTest.cpp` with
sequence-based assertions. Tests observe the sequence of `(state, value)` pairs
emitted by the cover and compare against an expected sequence built from helper
calls. Per-round timing branches (`isDebouncing`, `isStopDebouncing`,
`if (time <= 10000) ... else if ... else ...`) are removed.

---

## Problem

`CoverTest.cpp` (1317 lines) currently asserts state at every loop iteration
using branches on `time` and `round`. This produces:
- Long, deeply nested per-iteration conditionals (e.g. `Calibrate` is one ~290
  line function with 4 phases and 5–7 branches per phase).
- Per-test logic that depends on the exact loop period (`delay` parameter).
  Changing the test loop period requires re-deriving timing math.
- Failure messages that point at a single round; debugging requires correlating
  with the test source.

The cover's externally observable behavior is the sequence of `Actions::fire`
emissions (i.e. `interface.storedValue` updates). Asserting on that sequence
directly removes all timing branches.

---

## Design

### `loopFor` return value

`loopFor` runs the simulation loop and returns the sequence of state changes
observed during that run.

```cpp
std::vector<std::pair<std::string, std::string>> loopFor(
    unsigned long time, unsigned long delay);
```

Each `pair` is `(state, value)`. `value == ""` represents a state-only emission
(`storedValue.size() == 1`). The first emission observed in a test is captured;
pre-existing `storedValue` (e.g. initial `["CLOSED"]` from `init + loop()`) is
not included in the returned sequence.

Implementation notes:
- Capture the pre-loopFor `(state, value)` derived from `storedValue` as the
  baseline. Do **not** include it in the result.
- Initialize a "last seen" variable to this baseline.
- For each loop iteration:
  1. Call `this->loop()`.
  2. Re-read the tail of `storedValue` to derive the current `(state, value)`:
     - `size() == 0`: no current state, skip.
     - `size() == 1`: `current = {storedValue[0], ""}`.
     - `size() >= 2`: `current = {storedValue[size-2], storedValue[size-1]}`.
  3. If `current` differs from `last seen`, append it to the result and update
     `last seen` to `current`.
- The signature drops the per-iteration `func` callback.

### Helper functions (free, anonymous namespace, `test/CoverTest.cpp`)

```cpp
void addState(
    std::vector<std::pair<std::string, std::string>>& out,
    const std::string& state, const std::string& value = "");

void createSequence(
    std::vector<std::pair<std::string, std::string>>& out,
    const std::string& state, int from, int to, int step = 1);

std::string diff(
    const std::vector<std::pair<std::string, std::string>>& actual,
    const std::vector<std::pair<std::string, std::string>>& expected);
```

- `addState` appends a single entry; default `value = ""` covers state-only
  emissions.
- `createSequence` appends entries `(state, str(i))` for `i` in
  `from, from+step, ..., to`. Supports both ascending (`step = 1`, default) and
  descending (`step = -1`) sequences. Example: `createSequence(out, "OPENING", 1, 99)`
  appends 99 entries from `("OPENING", "1")` to `("OPENING", "99")`.
- `diff` returns a human-readable multi-line string for use as the
  `::testing::AssertionFailure` payload. Format:
  - One line per index where actual and expected differ:
    `  [i] actual=(state,value) expected=(state,value)`
  - `actual` and `expected` may have different lengths; trailing entries are
    flagged `(extra)` or `(missing)`.
  - Empty `value` rendered as `_` for readability.
  - If the sequences are equal, returns `""` (gtest omits the message).

### Test pattern

Single `loopFor` (most tests):

```cpp
this->open();
auto actual = this->loopFor(11000, delay);  // 10000ms travel + 1000ms buffer
std::vector<std::pair<std::string, std::string>> expected;
addState(expected, "OPENING");
addState(expected, "OPENING", "1");
if (hasPositionSensor) {
    addState(expected, "OPENING", "100");
} else {
    addState(expected, "CLOSED", "1");
}
addState(expected, "OPEN", "100");
EXPECT_EQ(actual, expected) << diff(actual, expected);
```

Multi-`loopFor` (manipulations between observations):

```cpp
this->setPosition(50);
auto a1 = this->loopFor(1000, delay);
std::vector<std::pair<std::string, std::string>> e1 = {
    {"OPENING", ""}, {"OPENING", "1"}};
EXPECT_EQ(a1, e1) << diff(a1, e1);

this->stop();
this->esp.delay(delay);
this->loop();

auto a2 = this->loopFor(delay * 3, delay);
std::vector<std::pair<std::string, std::string>> e2 = {
    {"OPEN", "1"}};
EXPECT_EQ(a2, e2) << diff(a2, e2);
```

`Calibrate` uses a single `loopFor` (no mid-test manipulations; the test
observes the full calibration flow continuously). Tests like
`StopMomentarilyWhileCalibrating` (which call `stop()` between observations)
use separate `loopFor` calls with the expectation reset between them.

### Timing buffer for automatic-stop tests

When the expectation is that the cover stops automatically (reaches the end
of travel, not stopped by a `stop()` command in the test), `loopFor` should be
called with extra time beyond the expected travel time. Example: a full open
from 0 to 100 takes ~10000 ms, so the test calls `loopFor(11000, delay)`
instead of `loopFor(10100, delay)`.

Rationale:
- The sequence of `storedValue` updates does not change with the extra time.
  The buffer just gives the cover enough iterations to fully settle into its
  end state and emit the final transition.
- Different `delay` values can cause the cover to reach the end at slightly
  different times relative to the loop period. The buffer absorbs this
  variability, so the same expected sequence works across all `delay`
  variants without per-variant time adjustments.
- The buffer must be at least one full stop debounce window (~20 ms) plus a
  margin for loop granularity. A 1000 ms buffer (10% over a 10 s travel) is
  the default for end-of-travel assertions; shorter buffers (e.g. 100–200 ms)
  are fine for sub-second phases.

Tests that do **not** need the buffer:
- `StopWhileOpening` / `StopWhileClosing`: the cover is stopped manually; the
  loopFor duration is the time before and after the manual stop, not the
  full travel time.
- `OpenAfterCalibrate` / `CloseAfterCalibrate`: the calibrated travel time
  is short; the buffer should be calibrated to the test's actual phase
  duration (≈ calibrated travel time + 200 ms).
- `StopEarlyWhileCalibrating` / `StopMomentarilyWhileCalibrating`: the cover
  is stopped manually after a short observation window.
- `CalibrationFailsIfMovementCannotStart`: oscillation pattern is observed;
  duration is `5 * t1` which is already generous.

### What stays

- `reboot()`, `init()`, `loop()`, `open()`, `close()`, `stop()`, `setPosition()`:
  unchanged.
- `calibrateToPosition()`: kept as a setup helper for tests that need a
  calibrated cover (e.g. `OpenAfterCalibrate`). Internally it still runs a
  `loopFor` but does not assert on the calibration sequence; only the final
  state is checked. (This is setup, not the test's primary assertion.)
- `this->position` direct checks: kept where the test verifies a numerical end
  state that is not also represented in the sequence (e.g.
  `EXPECT_EQ(this->position, 2000)` after stopping mid-movement).
- `reboot` test variation handling: unchanged.
- The five test fixtures (`HasPositionSensorFixture`, `BasicFixture`,
  `CalibrateFixture`, `MultiplePositionSensorsFixture`,
  `StopMomentarilyWhileCalibratingFixture`): unchanged.

### What goes away

- `isDebouncing(time, round)`: removed.
- `isStopDebouncing(time, round, endTime, delay)`: removed.
- The per-iteration `func` parameter on `loopFor`.
- Pin-state checks (`EXPECT_EQ(this->esp.digitalRead(UpOutput), 1)` etc.) in
  `NormalMode` and `LatchingMode` tests: the pin state is implicitly covered by
  the state transitions in the sequence. `UpOutput`/`DownOutput`/`StopOutput`
  reads remain available for tests that genuinely need them.
- The verbose `if (round == 1) ... else ...` branches in `Calibrate`,
  `CalibrationFailsIfMovementCannotStart`, `MultiplePositionSensors`, etc.
- `ASSERT_NO_FAILURE()` calls between every `loopFor` in `Calibrate` (the
  single `EXPECT_EQ` after the unified `loopFor` covers all assertions).

---

## Expected Sequences (Reference)

The new tests construct expected sequences from these patterns. These are
derived from the current behavior; the implementation phase verifies they are
correct.

### `Open` test (delay=10..500, isLatching=*, hasPositionSensor)

`!hasPositionSensor`:
```
[("OPENING", ""), ("OPENING", "1"),
 ("CLOSED", "1"), ("OPEN", "100")]
```

`hasPositionSensor`:
```
[("OPENING", ""), ("OPENING", "1"),
 ("OPENING", "100"), ("OPEN", "100")]
```

### `Close` test

Mirror of `Open` with `CLOSING`/`CLOSED` swapped in.

### `OpenWhileFullyOpen` / `CloseWhileFullyClosed`

No state change because the cover is already at the target. The cover is
positioned at the boundary (`this->position = 10000` for `OpenWhileFullyOpen`,
`this->position = 0` for `CloseWhileFullyClosed`) before `open()`/`close()`.
The movement detection starts but the motor input never changes (boundary
already reached), so the cover is stopped during the start timeout window.
Expected sequence: no `OPENING`/`CLOSING` emission; the cover returns to the
same end state. Implementation phase verifies the exact sequence (which may
include a brief `OPENING` followed by `OPEN` if the start timeout fires after
the boundary check).

### `NormalMode` / `LatchingMode`

These tests issue a sequence of commands (`open`, `close`, `stop`,
combinations) and verify the resulting pin states. With pin-state checks
dropped, the tests are restructured to observe the sequence of `storedValue`
changes after each command. Each command is followed by `this->loop()` to
trigger emission, then the new `(state, value)` is captured. The combined
sequence across the command sequence is asserted against an expected list.
The test name and parameterization are preserved.

### `StopWhileOpening` / `StopWhileClosing`

Two phases:
1. `loopFor(2000, delay)` while opening/closing.
2. `stop(); esp.delay(delay); loop(); loopFor(delay*3, delay)`.
   Expected phase 2: state settles to `OPEN` or `CLOSED` with the position
   where the cover was stopped.

### `Calibrate` test

Builds the expected sequence from the four phases described in the test
comment, with variants:

- `start = 0, hasPositionSensor = true`:
  - Phase 1: opening ends at sensor 100 (time = 10000ms, position known).
  - Phase 2: closing full travel; close time calibrated.
  - Phase 3: **skipped** (open time already known from sensor 0 + phase 2).
  - Phase 4: opens to 40 (no close in this branch).
- `start = 10000, hasPositionSensor = true`:
  - Phase 1: **skipped** (already at top).
  - Phase 2: closes (but up movement was skipped, so the close transitions are
    observed from a fresh start).
  - Phase 3: opens full travel.
  - Phase 4: closes to 40.
- `start = mid, hasPositionSensor = *`:
  - All four phases run.
- `!hasPositionSensor`:
  - All four phases run, intermediate position values are 1 (or 99) the whole
    time; only the final state value changes.

`createSequence` is the primary construction tool for the movement phases. For
example, a full OPENING 0→100 sequence in phase 1:

```cpp
addState(expected, "OPENING");
addState(expected, "OPENING", "1");
// ... (no further OPENING values emitted for !hasPositionSensor)
// For hasPositionSensor:
addState(expected, "OPENING", "100");
addState(expected, "OPEN", "100");
```

For phase 2 (close 100→0) with position sensor:
```cpp
addState(expected, "CLOSING", "100");
createSequence(expected, "CLOSING", 99, 1, -1);
addState(expected, "CLOSING", "0");
addState(expected, "CLOSED", "0");
```

### `MultiplePositionSensors`

Single `loopFor` for the full sequence of opening and closing alternations.
Helper functions express the four phases (open uncalibrated, close
uncalibrated, open calibrated, close calibrated).

### `StopEarlyWhileCalibrating`, `StopMomentarilyWhileCalibrating`

`setPosition(50)`, `loopFor(1000, delay)` to observe opening start, then
`stop()`, then second `loopFor(delay * 3, delay)`. The two phases use separate
expected sequences.

### `CalibrationFailsIfMovementCannotStart`

`setPosition(50)` with `isWorking = false`. Single `loopFor` observing
oscillation between up/down with no state progression. Expected sequence
includes the alternating up/down emission pattern.

---

## File Changes

### Modified files

| File | Change |
|------|--------|
| `test/CoverTest.cpp` | Major refactor: new `loopFor` signature, helper functions, all tests rewritten to use sequence assertions. |

### Unchanged files

- `src/**`: no changes.
- `test/CoverMovementTest.cpp`, `test/CoverStopTest.cpp`, `test/CoverUpdateTest.cpp`:
  unaffected; they already use unit-style assertions, not timing.
- All other test files: unaffected.

---

## Test Strategy

### Implementation verification

After refactoring, run:
```
./build/home_automation_test --gtest_filter=CoverTest*
```

The test count and the set of parameterized variants are unchanged. Each
existing test name must still pass with the new sequence-based assertions.

### Debug workflow

When a test fails, gtest prints `diff(actual, expected)` from the
`::testing::AssertionFailure << diff(...)` payload. This shows the
disagreement index-by-index without needing to re-read the test source.

### Variants

Variant differences in the expected sequence are expressed directly in the
test body using `if (hasPositionSensor) ... else ...` branches and the
helper functions. The test source remains the documentation of what each
variant expects.

---

## Constraints

- C++17, no exceptions in device code (test code may use exceptions indirectly
  through gtest).
- Instance members prefixed with `this->`.
- `EspApi` used for all hardware calls in testable code.
- `clang-format` for any new code.
- Build (`arduino-cli compile --fqbn esp8266:esp8266:generic --verify`) and
  test (`./build/home_automation_test --gtest_filter=CoverTest*`) after every
  change.
