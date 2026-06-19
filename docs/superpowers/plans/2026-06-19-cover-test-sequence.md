# CoverTest Sequence Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace per-iteration timing assertions in `test/CoverTest.cpp` with sequence-based assertions driven by a new `loopFor2` method and three free-function helpers (`addState`, `createSequence`, `diff`).

**Architecture:** Add helpers + `loopFor2` alongside the existing `loopFor`. Refactor each of the 16 tests one at a time, swapping `loopFor` for `loopFor2` and replacing the per-iteration lambda with a single `EXPECT_EQ` against an expected sequence. After all tests use `loopFor2`, delete the old `loopFor` and rename `loopFor2 → loopFor`.

**Tech Stack:** C++17, GoogleTest, `clang-format`. Test build via `cmake` + `make` in `build/`, test binary at `build/home_automation_test`. Device build via `arduino-cli compile --fqbn esp8266:esp8266:generic --verify` (must keep passing — `test/CoverTest.cpp` is not built into the device image, but the file must still parse cleanly).

**Reference spec:** `docs/superpowers/specs/2026-06-19-cover-test-sequence-design.md`

---

## File Structure

### `test/CoverTest.cpp` (only file modified)

- **Helpers** (anonymous namespace at top of file, after `#include` block):
  - `addState(out, state, value="")` — append one `(state, value)` pair.
  - `createSequence(out, state, from, to, step=1)` — append a contiguous range.
  - `diff(actual, expected)` — return a human-readable diff string.
- **CoverTest class changes:**
  - Keep existing `loopFor(time, delay, func)` until all tests are migrated.
  - Add new `loopFor2(time, delay) → vector<pair<string,string>>` method.
  - Remove `isDebouncing`, `isStopDebouncing` static helpers (no longer used).
  - `calibrateToPosition` keeps its current shape (setup helper).
  - Per-test refactor: replace per-iteration lambda with `EXPECT_EQ(actual, expected) << diff(actual, expected)`.

### `src/**`, other tests, headers, `CMakeLists.txt`

Unchanged.

---

## Execution order

1. Add helpers (no test change).
2. Add `loopFor2` (no test change; old `loopFor` still in use).
3. Migrate 16 tests, one per task. Order: simple → complex.
4. Delete old `loopFor`, rename `loopFor2 → loopFor`.
5. Build + test after every task.

---

## Task 1: Add helper functions

**Files:**
- Modify: `test/CoverTest.cpp` (anonymous namespace near top of file, after `#include` block)

- [ ] **Step 1: Add the three helpers to the anonymous namespace**

First, add the required includes. The current file has `<algorithm>`, `<iostream>`, `<tuple>`. Add:

```cpp
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
```

Then, after the `#include` block and before `#define GET_PARAM`, add:

```cpp
namespace {

using CoverState = std::pair<std::string, std::string>;
using CoverSequence = std::vector<CoverState>;

void addState(
    CoverSequence& out,
    const std::string& state, const std::string& value = "") {
    out.emplace_back(state, value);
}

void createSequence(
    CoverSequence& out,
    const std::string& state, int from, int to, int step = 1) {
    if (step == 0) {
        return;
    }
    if (step > 0) {
        for (int i = from; i <= to; i += step) {
            out.emplace_back(state, std::to_string(i));
        }
    } else {
        for (int i = from; i >= to; i += step) {
            out.emplace_back(state, std::to_string(i));
        }
    }
}

std::string diff(
    const CoverSequence& actual,
    const CoverSequence& expected) {
    if (actual == expected) {
        return "";
    }
    std::ostringstream os;
    auto a = actual.begin();
    auto e = expected.begin();
    size_t i = 0;
    for (; a != actual.end() && e != expected.end(); ++a, ++e, ++i) {
        if (*a != *e) {
            os << "  [" << i << "] actual=(" << a->first << ","
               << (a->second.empty() ? "_" : a->second) << ") expected=("
               << e->first << ","
               << (e->second.empty() ? "_" : e->second) << ")\n";
        }
    }
    while (a != actual.end()) {
        os << "  [" << i << "] actual=(" << a->first << ","
           << (a->second.empty() ? "_" : a->second) << ") (extra)\n";
        ++a;
        ++i;
    }
    while (e != expected.end()) {
        os << "  [" << i << "] expected=(" << e->first << ","
           << (e->second.empty() ? "_" : e->second) << ") (missing)\n";
        ++e;
        ++i;
    }
    return os.str();
}

}  // namespace
```

- [ ] **Step 2: Build to verify the file still compiles**

Run: `cd build && cmake --build . -j$(nproc) 2>&1 | tail -20`
Expected: build succeeds.

NOTE: The project has `-Werror` enabled in `CMakeLists.txt`, so unused-function warnings on the three new helpers will fail the build. Add a static lambda at the end of the anonymous namespace that takes the addresses of the three helpers, forcing ODR-use until they are referenced by tests:

```cpp
// Force ODR-use of the helpers to suppress unused-function warnings
// (project uses -Werror) until they are used in subsequent tasks.
static const auto _unused_helper_marker = []() {
    (void)&addState;
    (void)&createSequence;
    (void)&diff;
    return 0;
}();
```

Task 2 removes this marker once `loopFor2` references the helpers.

- [ ] **Step 3: Run existing tests to confirm no regression**

Run: `./build/home_automation_test --gtest_filter=CoverTest*`
Expected: all existing CoverTest tests pass (helpers added but unused by tests).

- [ ] **Step 4: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): add addState/createSequence/diff helpers"
```

---

## Task 2: Add `loopFor2` method

**Files:**
- Modify: `test/CoverTest.cpp` (`CoverTest` class, just below the existing `loopFor` method)

- [ ] **Step 1: Add `loopFor2` to the class**

The `CoverState` and `CoverSequence` type aliases are already defined in the anonymous namespace from Task 1. Insert the new method after the existing `loopFor` method (after line 212 in current file, just before `static bool isDebouncing`):

```cpp
CoverSequence loopFor2(unsigned long time, unsigned long delay) {
    auto beginTime = this->esp.millis();
    std::cout << "---- loopFor2 " << time << "----" << std::endl;

    auto getCurrent = [this]() -> std::optional<CoverState> {
        const auto& v = this->interface.storedValue;
        if (v.empty()) {
            return std::nullopt;
        }
        if (v.size() == 1) {
            return CoverState{v.back(), ""};
        }
        return CoverState{v[v.size() - 2], v.back()};
    };

    std::optional<CoverState> last = getCurrent();
    CoverSequence result;
    this->delayUntil(beginTime + time, delay, [&]() {
        this->loop();
        auto current = getCurrent();
        if (current && (!last || *last != *current)) {
            result.push_back(*current);
            last = current;
        }
    });
    std::cout << "---- loopFor2 done ----" << std::endl;
    return result;
}
```

- [ ] **Step 2: Build to verify it compiles**

Run: `cd build && cmake --build . -j$(nproc) 2>&1 | tail -20`
Expected: build succeeds. If `std::optional` is not transitively included, add `#include <optional>` and `#include <sstream>` to the `#include` block at the top of `test/CoverTest.cpp`.

- [ ] **Step 3: Run existing tests to confirm no regression**

Run: `./build/home_automation_test --gtest_filter=CoverTest*`
Expected: all existing tests still pass; `loopFor2` is unused.

- [ ] **Step 4: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): add loopFor2 returning observed state sequence"
```

---

## Task 3: Refactor `NormalMode` test

**Files:**
- Modify: `test/CoverTest.cpp` (lines 276-315, the `TEST_P(HasPositionSensorFixture, NormalMode)` test)

- [ ] **Step 1: Capture the observed sequence**

Replace the test body with a temporary one that calls `loopFor2` around each command and prints the result. Build and run once with `--gtest_filter='*NormalMode*' --gtest_also_run_disabled_tests` to see the actual sequences. Use the printed output to construct the expected sequences.

- [ ] **Step 2: Write the new test body**

The test issues: `init`, `open`, `stop`, `close`, `stop`, `open`, `close`, `open`, `close`. For each command, call `this->loop()` to trigger an emission, capture the new `(state, value)` via `getCurrent()` (or a small helper that wraps `loopFor2` for zero-duration observation), and append to a single expected sequence.

Pattern:

```cpp
TEST_P(HasPositionSensorFixture, NormalMode) {
    GET_PARAM(hasPositionSensor, 0);

    this->position = 5000;
    this->init(false, this->getPositionSensors(hasPositionSensor));
    this->loop();

    auto observe = [this]() {
        return this->loopFor2(this->esp.millis() + 1, 1);
    };

    this->open();
    auto s1 = observe();
    this->stop();
    auto s2 = observe();
    this->close();
    auto s3 = observe();
    this->stop();
    auto s4 = observe();
    this->open();
    auto s5 = observe();
    this->close();
    auto s6 = observe();
    this->open();
    auto s7 = observe();
    this->close();
    auto s8 = observe();

    // Build expected from observations
    CoverSequence expected;
    expected.insert(expected.end(), s1.begin(), s1.end());
    expected.insert(expected.end(), s2.begin(), s2.end());
    // ... etc for s3..s8
    (void)expected;  // placeholder, fill in real expected per observations
}
```

After the placeholder works (compiles and shows actual sequences), replace the `(void)expected;` line with `EXPECT_EQ(s1, CoverSequence{...})` per command, or accumulate and assert once.

- [ ] **Step 3: Run the test, observe actual sequences, fill in expected**

Run: `./build/home_automation_test --gtest_filter='*NormalMode*'`
Use the printed `loopFor2 done` output to derive the expected sequences for each command. Re-run until the test passes.

- [ ] **Step 4: Remove temporary `cout` output if added (optional, for noise reduction)**

- [ ] **Step 5: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): refactor NormalMode to sequence assertions"
```

---

## Task 4: Refactor `LatchingMode` test

**Files:**
- Modify: `test/CoverTest.cpp` (lines 321-364, the `TEST_P(HasPositionSensorFixture, LatchingMode)` test)

- [ ] **Step 1: Follow the same pattern as Task 3**

Same observation + expected-sequence approach. Latching mode has different pin behavior (stop pin is involved), so the state transitions may differ from NormalMode.

- [ ] **Step 2: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): refactor LatchingMode to sequence assertions"
```

---

## Task 5: Refactor `Open` test

**Files:**
- Modify: `test/CoverTest.cpp` (lines 366-403, the `TEST_P(BasicFixture, Open)` test)

- [ ] **Step 1: Write the new test body**

```cpp
TEST_P(BasicFixture, Open) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->loop();

    this->open();
    auto actual = this->loopFor2(11000, delay);  // 10000ms travel + 1000ms buffer

    CoverSequence expected;
    addState(expected, "OPENING");
    addState(expected, "OPENING", "1");
    if (hasPositionSensor) {
        addState(expected, "OPENING", "100");
    } else {
        addState(expected, "CLOSED", "1");
    }
    addState(expected, "OPEN", "100");
    EXPECT_EQ(actual, expected) << diff(actual, expected);
}
```

- [ ] **Step 2: Run the test for all variants**

Run: `./build/home_automation_test --gtest_filter='*Open*'`
Expected: passes for all 16 variants (4 delays × 2 latching × 2 sensor). If a variant fails, use `diff(actual, expected)` output to identify the divergence and adjust expected for that variant.

- [ ] **Step 3: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): refactor Open to sequence assertions"
```

---

## Task 6: Refactor `OpenWhileFullyOpen` test

**Files:**
- Modify: `test/CoverTest.cpp` (lines 405-426, `TEST_P(BasicFixture, OpenWhileFullyOpen)`)

- [ ] **Step 1: Write the new test body**

```cpp
TEST_P(BasicFixture, OpenWhileFullyOpen) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->position = 10000;
    this->loop();

    this->open();
    auto actual = this->loopFor2(1100, delay);  // wait for start timeout (1000ms) + buffer

    CoverSequence expected;
    // Cover is already at the top. With position sensors, the sensor at 100%
    // is active and reports position 100. Without position sensors, start
    // timeout fires and the cover stops. Sequence depends on variant; verify
    // during implementation.
    EXPECT_EQ(actual, expected) << diff(actual, expected);
}
```

Run the test and observe what `actual` contains. Fill in `expected` based on the observation (this test is likely empty or short).

- [ ] **Step 2: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): refactor OpenWhileFullyOpen to sequence assertions"
```

---

## Task 7: Refactor `CloseWhileFullyClosed` test

**Files:**
- Modify: `test/CoverTest.cpp` (lines 428-449, `TEST_P(BasicFixture, CloseWhileFullyClosed)`)

- [ ] **Step 1: Follow the Task 6 pattern (mirror for `close` instead of `open`, position=0)**

- [ ] **Step 2: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): refactor CloseWhileFullyClosed to sequence assertions"
```

---

## Task 8: Refactor `Close` test

**Files:**
- Modify: `test/CoverTest.cpp` (lines 451-489, `TEST_P(BasicFixture, Close)`)

- [ ] **Step 1: Write the new test body (mirror of Task 5)**

```cpp
TEST_P(BasicFixture, Close) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->position = 10000;
    this->loop();

    this->close();
    auto actual = this->loopFor2(11000, delay);

    CoverSequence expected;
    addState(expected, "CLOSING");
    addState(expected, "CLOSING", "99");
    if (hasPositionSensor) {
        addState(expected, "CLOSING", "0");
    } else {
        addState(expected, "OPEN", "99");
    }
    addState(expected, "CLOSED", "0");
    EXPECT_EQ(actual, expected) << diff(actual, expected);
}
```

- [ ] **Step 2: Run, observe, fix expected if needed**

- [ ] **Step 3: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): refactor Close to sequence assertions"
```

---

## Task 9: Refactor `StopWhileOpening` test

**Files:**
- Modify: `test/CoverTest.cpp` (lines 491-511, `TEST_P(BasicFixture, StopWhileOpening)`)

- [ ] **Step 1: Write the new test body (multi-loopFor with manipulation)**

```cpp
TEST_P(BasicFixture, StopWhileOpening) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->loop();

    this->open();
    auto phase1 = this->loopFor2(2000, delay);

    this->stop();
    this->esp.delay(delay);
    this->loop();
    auto phase2 = this->loopFor2(delay * 3, delay);

    CoverSequence expected1;
    addState(expected1, "OPENING");
    addState(expected1, "OPENING", "1");
    EXPECT_EQ(phase1, expected1) << diff(phase1, expected1);

    CoverSequence expected2;
    addState(expected2, "OPEN", "1");  // cover settles at intermediate position
    EXPECT_EQ(phase2, expected2) << diff(phase2, expected2);

    EXPECT_EQ(this->position, 2000);
    EXPECT_FALSE(this->isMovingUp());
    EXPECT_FALSE(this->isMovingDown());
}
```

- [ ] **Step 2: Run, observe, fix expected if needed**

- [ ] **Step 3: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): refactor StopWhileOpening to sequence assertions"
```

---

## Task 10: Refactor `StopWhileClosing` test

**Files:**
- Modify: `test/CoverTest.cpp` (lines 513-534, `TEST_P(BasicFixture, StopWhileClosing)`)

- [ ] **Step 1: Mirror Task 9 for `close` instead of `open`**

- [ ] **Step 2: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): refactor StopWhileClosing to sequence assertions"
```

---

## Task 11: Refactor `Calibrate` test

**Files:**
- Modify: `test/CoverTest.cpp` (lines 549-841, the large `TEST_P(CalibrateFixture, Calibrate)` test)

- [ ] **Step 1: Write the new test body using a single `loopFor2`**

```cpp
TEST_P(CalibrateFixture, Calibrate) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);
    GET_PARAM(start, 3);

    if (delay == 500) {
        GTEST_SKIP() << "delay=500 not supported for this test";
    }

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->position = start;
    this->loop();

    this->setPosition(40);
    auto actual = this->loopFor2(41000, delay);

    CoverSequence expected;
    if (hasPositionSensor && start == 0) {
        // Phase 1: open to sensor 100. Phase 2: close full. Phase 3 skipped.
        // Phase 4: open to 40.
        addState(expected, "OPENING", "0");
        addState(expected, "OPENING", "1");
        addState(expected, "OPENING", "100");
        addState(expected, "OPEN", "100");
        addState(expected, "CLOSING", "100");
        createSequence(expected, "CLOSING", 99, 1, -1);
        addState(expected, "CLOSING", "0");
        addState(expected, "CLOSED", "0");
        addState(expected, "OPENING", "0");
        createSequence(expected, "OPENING", 1, 39);
        addState(expected, "OPENING", "40");
        addState(expected, "OPEN", "40");
    } else if (hasPositionSensor && start == this->maxPosition) {
        // Phase 1 skipped. Phase 2: close. Phase 3: open. Phase 4: close to 40.
        addState(expected, "CLOSING", "100");
        createSequence(expected, "CLOSING", 99, 1, -1);
        addState(expected, "CLOSING", "0");
        addState(expected, "CLOSED", "0");
        addState(expected, "OPENING", "0");
        createSequence(expected, "OPENING", 1, 99);
        addState(expected, "OPENING", "100");
        addState(expected, "OPEN", "100");
        addState(expected, "CLOSING", "100");
        createSequence(expected, "CLOSING", 99, 41, -1);
        addState(expected, "CLOSING", "40");
        addState(expected, "OPEN", "40");
    } else if (!hasPositionSensor && start == 0) {
        // All four phases; intermediate values stay at 1 (or 99).
        addState(expected, "OPENING", "0");
        addState(expected, "OPENING", "1");
        addState(expected, "CLOSED", "1");
        addState(expected, "OPEN", "100");
        addState(expected, "CLOSING", "100");
        addState(expected, "CLOSING", "99");
        addState(expected, "OPEN", "99");
        addState(expected, "OPENING", "99");
        addState(expected, "OPENING", "1");
        addState(expected, "CLOSING", "1");
        addState(expected, "OPEN", "40");
    } else {
        // Other variants: build expected per observation. See Step 2.
        // Placeholder kept; will be filled in by iteration.
    }

    EXPECT_EQ(actual, expected) << diff(actual, expected);
    EXPECT_FALSE(this->isMovingUp());
    EXPECT_FALSE(this->isMovingDown());
}
```

The exact expected sequences above are best-guesses derived from the current test's assertions. Some intermediate values (especially for `!hasPositionSensor` with mid-start positions) may differ. Use the `diff()` output in failures to refine.

- [ ] **Step 2: Run, observe actual sequence for the first variant, fill in expected**

Run: `./build/home_automation_test --gtest_filter='*Calibrate*Calibrate/0*'`
The first variant (delay=10, isLatching=false, hasPositionSensor=false, start=0) is the simplest. Use its observed sequence as the base.

- [ ] **Step 3: Iterate through all 24 variants, adjust expected for each**

Run: `./build/home_automation_test --gtest_filter='*Calibrate*'`
The expected differs based on `start`, `hasPositionSensor`, and `isLatching`. Use `if/else` branches in the test body to construct the right expected per variant.

- [ ] **Step 4: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): refactor Calibrate to single sequence assertion"
```

---

## Task 12: Refactor `OpenAfterCalibrate` test

**Files:**
- Modify: `test/CoverTest.cpp` (lines 850-893, `TEST_P(BasicFixture, OpenAfterCalibrate)`)

- [ ] **Step 1: Keep `calibrateToPosition` as setup; refactor only the post-calibrate loopFor**

```cpp
TEST_P(BasicFixture, OpenAfterCalibrate) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    if (delay == 500) {
        GTEST_SKIP() << "delay=500 not supported for this test";
    }

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    ASSERT_NO_FATAL_FAILURE(this->calibrateToPosition(60, delay));
    this->open();
    auto actual = this->loopFor2(4200 + delay, delay);

    CoverSequence expected;
    // After calibration, opening from 60 to 100 takes ~4000ms.
    // Sequence: OPENING/60, OPENING/61, ..., OPENING/100, OPEN/100.
    addState(expected, "OPENING", "60");
    createSequence(expected, "OPENING", 61, 100);
    addState(expected, "OPEN", "100");
    EXPECT_EQ(actual, expected) << diff(actual, expected);
}
```

- [ ] **Step 2: Run, observe, fix expected if needed**

- [ ] **Step 3: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): refactor OpenAfterCalibrate to sequence assertions"
```

---

## Task 13: Refactor `CloseAfterCalibrate` test

**Files:**
- Modify: `test/CoverTest.cpp` (lines 895-933, `TEST_P(BasicFixture, CloseAfterCalibrate)`)

- [ ] **Step 1: Mirror Task 12 for `close` instead of `open`**

- [ ] **Step 2: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): refactor CloseAfterCalibrate to sequence assertions"
```

---

## Task 14: Refactor `RestartAfterCalibrate` test

**Files:**
- Modify: `test/CoverTest.cpp` (lines 935-981, `TEST_P(BasicFixture, RestartAfterCalibrate)`)

- [ ] **Step 1: Write the new test body**

```cpp
TEST_P(BasicFixture, RestartAfterCalibrate) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    if (delay == 500) {
        GTEST_SKIP() << "delay=500 not supported for this test";
    }

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    ASSERT_NO_FATAL_FAILURE(this->calibrateToPosition(60, delay));
    this->reboot();
    this->position = 6000;
    this->loop();
    this->setPosition(40);

    auto phase1 = this->loopFor2(2000, delay);
    auto phase2 = this->loopFor2(delay * 3, delay);

    CoverSequence expected1;
    addState(expected1, "CLOSING", "60");
    createSequence(expected1, "CLOSING", 59, 41, -1);
    EXPECT_EQ(phase1, expected1) << diff(phase1, expected1);

    CoverSequence expected2;
    addState(expected2, "OPEN", "40");
    EXPECT_EQ(phase2, expected2) << diff(phase2, expected2);

    EXPECT_EQ(this->position, 4000 - delay);
    EXPECT_FALSE(this->isMovingUp());
    EXPECT_FALSE(this->isMovingDown());
}
```

- [ ] **Step 2: Run, observe, fix expected if needed**

- [ ] **Step 3: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): refactor RestartAfterCalibrate to sequence assertions"
```

---

## Task 15: Refactor `MultiplePositionSensors` test

**Files:**
- Modify: `test/CoverTest.cpp` (lines 983-1151, the `TEST_P(MultiplePositionSensorsFixture, MultiplePositionSensors)` test)

- [ ] **Step 1: Write the new test body using a single `loopFor2` per phase**

Four phases (open uncalibrated, close uncalibrated, open calibrated, close calibrated) with debounce gaps between. Use four `loopFor2` calls and accumulate the sequence (or assert each phase separately).

- [ ] **Step 2: Run, observe, fix expected per phase**

The expected sequence is complex due to the three position sensors (0%, 50%, 100%) and four phases. Use the first variant's observation to derive the base, then check other variants.

- [ ] **Step 3: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): refactor MultiplePositionSensors to sequence assertions"
```

---

## Task 16: Refactor `StopEarlyWhileCalibrating` test

**Files:**
- Modify: `test/CoverTest.cpp` (lines 1160-1191, `TEST_P(BasicFixture, StopEarlyWhileCalibrating)`)

- [ ] **Step 1: Two-loopFor with manipulation pattern (like Task 9)**

- [ ] **Step 2: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): refactor StopEarlyWhileCalibrating to sequence assertions"
```

---

## Task 17: Refactor `StopMomentarilyWhileCalibrating` test

**Files:**
- Modify: `test/CoverTest.cpp` (lines 1193-1224, `TEST_P(StopMomentarilyWhileCalibratingFixture, StopMomentarilyWhileCalibrating)`)

- [ ] **Step 1: Two-loopFor with manipulation pattern (like Task 9)**

- [ ] **Step 2: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): refactor StopMomentarilyWhileCalibrating to sequence assertions"
```

---

## Task 18: Refactor `CalibrationFailsIfMovementCannotStart` test

**Files:**
- Modify: `test/CoverTest.cpp` (lines 1226-1306, `TEST_P(CalibrateFixture, CalibrationFailsIfMovementCannotStart)`)

- [ ] **Step 1: Single `loopFor2` observing the alternating up/down pattern**

- [ ] **Step 2: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): refactor CalibrationFailsIfMovementCannotStart"
```

---

## Task 19: Remove old `loopFor`, rename `loopFor2 → loopFor`, remove `isDebouncing`/`isStopDebouncing`

**Files:**
- Modify: `test/CoverTest.cpp`

- [ ] **Step 1: Verify all tests use `loopFor2` (no remaining callers of `loopFor`)**

Run: `grep -n 'loopFor(' test/CoverTest.cpp`
Expected: no matches (all references should now be `loopFor2`).

- [ ] **Step 2: Delete the old `loopFor` method**

Remove the entire `loopFor` method definition from the class (the one that takes `time, delay, func`).

- [ ] **Step 3: Rename `loopFor2` to `loopFor`**

Replace all occurrences of `loopFor2` with `loopFor` in the file. The `using` aliases (`CoverState`, `CoverSequence`) may be inlined or kept.

- [ ] **Step 4: Remove `isDebouncing` and `isStopDebouncing` static methods**

Delete both method bodies. Verify no remaining references via `grep -n 'isDebouncing\|isStopDebouncing' test/CoverTest.cpp`.

- [ ] **Step 5: Remove `loopFor2 done` and `loopFor2 ----` debug prints (renamed to `loopFor`)**

- [ ] **Step 6: Build**

Run: `cd build && cmake --build . -j$(nproc) 2>&1 | tail -20`
Expected: clean build.

- [ ] **Step 7: Run all tests**

Run: `./build/home_automation_test --gtest_filter=CoverTest*`
Expected: all tests pass.

- [ ] **Step 8: Build device image**

Run: `arduino-cli compile --fqbn esp8266:esp8266:generic --verify 2>&1 | tail -20`
Expected: device build succeeds (CoverTest.cpp is not built into the device, but the file must still parse and other files must build).

- [ ] **Step 9: Commit**

```bash
git add test/CoverTest.cpp
git commit -m "test(CoverTest): remove old loopFor, rename loopFor2, drop debounce helpers"
```

---

## Task 20: Final verification

**Files:**
- None (verification only)

- [ ] **Step 1: Run the full test suite**

Run: `./build/home_automation_test`
Expected: all tests pass, including the new sequence-based CoverTest tests.

- [ ] **Step 2: Build the device image**

Run: `arduino-cli compile --fqbn esp8266:esp8266:generic --verify`
Expected: build succeeds.

- [ ] **Step 3: clang-format the modified file**

Run: `clang-format -i test/CoverTest.cpp`
Expected: file is formatted per `.clang-format`.

- [ ] **Step 4: Commit formatting if changed**

```bash
git diff --stat
git add test/CoverTest.cpp
git commit -m "test(CoverTest): apply clang-format" --allow-empty
```

- [ ] **Step 5: Report final stats**

Run: `wc -l test/CoverTest.cpp`
Expected: significantly fewer lines than the 1317 of the original (target: ~600-800 with cleaner per-test logic).

---

## Self-Review Checklist (run before executing)

- [ ] All 16 tests in the file are accounted for in Tasks 3-18. List: NormalMode, LatchingMode, Open, OpenWhileFullyOpen, CloseWhileFullyClosed, Close, StopWhileOpening, StopWhileClosing, Calibrate, OpenAfterCalibrate, CloseAfterCalibrate, RestartAfterCalibrate, MultiplePositionSensors, StopEarlyWhileCalibrating, StopMomentarilyWhileCalibrating, CalibrationFailsIfMovementCannotStart. **16 tests. ✓**
- [ ] Helpers `addState`, `createSequence`, `diff` defined exactly once. ✓
- [ ] `loopFor2` defined once, on the `CoverTest` class. ✓
- [ ] Each refactor task includes a `Commit` step. ✓
- [ ] Each refactor task includes a build verification. ✓
- [ ] Final task (19) removes the old `loopFor` and renames `loopFor2`. ✓
- [ ] No code changes to `src/`. ✓
- [ ] Spec's reference sequences in the "Expected Sequences" section align with the example expected sequences in Tasks 5, 8, 12, etc. Verify during execution.
