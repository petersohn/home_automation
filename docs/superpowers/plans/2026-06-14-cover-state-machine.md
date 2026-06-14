# Cover State-Machine Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce coupling between `Cover` and its components by introducing `CoverState` (focused struct owned by `Cover`), a `CoverStop` abstract base, `unique_ptr` ownership of components in `CoverUpdate`, and a new `requestOpen/Close/Stop/SetPosition` command surface on `CoverUpdate`. Behavior is unchanged; integration tests must pass without modification.

**Architecture:** `Cover` becomes a thin command dispatcher. `CoverState` replaces `CoverMovementContext` and is owned by `Cover`. `CoverUpdate` is the only object that touches `up`/`down`/`stopper` and owns them via `unique_ptr`. New abstract `CoverStop` base enables `FakeCoverStop` in tests with no `EspApi` dependency.

**Tech Stack:** C++17, Arduino/ESP8266 (device), GoogleTest (native tests), `std::unique_ptr` (header-only), `clang-format`.

**Working directory:** All paths are relative to the project root. Build commands assume you're in `build/`. The build picks up `src/common/*.cpp` and `test/*.cpp` automatically (CMake `GLOB`).

**Conventions:**
- Instance members prefixed with `this->`
- `clang-format` on all modified files
- `git commit` after each task

---

## File structure

| File | Role |
|------|------|
| `src/common/CoverState.hpp` (renamed from `CoverMovementContext.hpp`) | Runtime state + config + services struct, owned by `Cover` |
| `src/common/CoverStop.hpp` (new, abstract) | Pure interface for `stop()` / `reset()` / `isTriggered()` / `isLatching()` |
| `src/common/CoverStopImpl.hpp`/`.cpp` (renamed from `CoverStop.hpp`/`.cpp`) | Concrete `CoverStopImpl` inheriting `CoverStop` |
| `src/common/CoverMovementImpl.hpp`/`.cpp` | One-direction movement; takes `CoverState&` and `CoverStop&` |
| `src/common/CoverUpdate.hpp`/`.cpp` | Owns the three components via `unique_ptr`; exposes `request*` and `update` |
| `src/common/Cover.hpp`/`.cpp` | Thin dispatcher: command parsing, config validation, RTC persistence |
| `test/CoverTest.cpp` | Integration test; **unchanged** |
| `test/CoverMovementTest.cpp` | Unit tests for `CoverMovementImpl`; mechanical rename |
| `test/CoverUpdateTest.cpp` | Unit tests for `CoverUpdate`; uses `FakeCoverStop` and `FakeCoverMovement` |

---

## Task 1: Rename `CoverMovementContext` → `CoverState`

**Files:**
- Rename: `src/common/CoverMovementContext.hpp` → `src/common/CoverState.hpp`
- Modify: All files that `#include` the renamed header or reference the class

- [ ] **Step 1: Move the file with git**

```bash
git mv src/common/CoverMovementContext.hpp src/common/CoverState.hpp
```

- [ ] **Step 2: Rename the class in the file**

```bash
sed -i 's/\bCoverMovementContext\b/CoverState/g' src/common/CoverState.hpp
sed -i 's/COVER_MOVEMENT_CONTEXT_HPP/COVER_STATE_HPP/g' src/common/CoverState.hpp
```

- [ ] **Step 3: Update all references in source files**

Files to update: `src/common/Cover.hpp`, `src/common/Cover.cpp`, `src/common/CoverMovementImpl.hpp`, `src/common/CoverUpdate.hpp`, `test/CoverMovementTest.cpp`, `test/CoverUpdateTest.cpp`.

```bash
for f in \
  src/common/Cover.hpp \
  src/common/Cover.cpp \
  src/common/CoverMovementImpl.hpp \
  src/common/CoverUpdate.hpp \
  test/CoverMovementTest.cpp \
  test/CoverUpdateTest.cpp; do
  sed -i 's|CoverMovementContext\.hpp|CoverState.hpp|g' "$f"
  sed -i 's/\bCoverMovementContext\b/CoverState/g' "$f"
done
```

- [ ] **Step 4: Build and run all tests**

```bash
cd build && cmake && make -j$(nproc) && ./home_automation_test
```

Expected: build succeeds, all tests pass.

- [ ] **Step 5: Verify device build**

```bash
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
```

Expected: device build succeeds.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "Rename CoverMovementContext to CoverState"
```

---

## Task 2: Add abstract `CoverStop`; rename concrete `CoverStop` → `CoverStopImpl`

**Files:**
- Create: `src/common/CoverStop.hpp` (new, abstract)
- Rename: `src/common/CoverStop.hpp` → `src/common/CoverStopImpl.hpp`
- Rename: `src/common/CoverStop.cpp` → `src/common/CoverStopImpl.cpp`
- Modify: All files that include or reference the renamed class

- [ ] **Step 1: Create the abstract `CoverStop` base**

Write `src/common/CoverStop.hpp` with this content:

```cpp
#ifndef COVER_STOP_HPP
#define COVER_STOP_HPP

class CoverStop {
public:
    virtual ~CoverStop() = default;
    virtual void stop() = 0;
    virtual void reset() = 0;
    virtual bool isTriggered() const = 0;
    virtual bool isLatching() const = 0;
};

#endif  // COVER_STOP_HPP
```

- [ ] **Step 2: Rename the concrete files**

```bash
git mv src/common/CoverStop.hpp src/common/CoverStopImpl.hpp
git mv src/common/CoverStop.cpp src/common/CoverStopImpl.cpp
```

- [ ] **Step 3: Rename the class in the .hpp to `CoverStopImpl` and add inheritance**

```bash
sed -i 's/\bclass CoverStop\b/class CoverStopImpl/g' src/common/CoverStopImpl.hpp
```

Then read `src/common/CoverStopImpl.hpp` and add `: public CoverStop` to the class declaration. The result should be:

```cpp
class CoverStopImpl : public CoverStop {
public:
    // ... existing constructor ...
    void stop() override;
    void reset() override;
    bool isTriggered() const override;
    bool isLatching() const override;
private:
    // ... existing private members ...
};
```

(Add `override` to each method declaration. Verify includes for `<cstdint>`, `<ostream>`, `<string>`, `EspApi.hpp` are present.)

- [ ] **Step 4: Update the .cpp file references to the class name**

```bash
sed -i 's/\bCoverStop::/CoverStopImpl::/g' src/common/CoverStopImpl.cpp
```

- [ ] **Step 5: Update `src/common/Cover.cpp` to construct `CoverStopImpl`**

```bash
sed -i 's|#include "CoverStop.hpp"|#include "CoverStopImpl.hpp"|' src/common/Cover.cpp
sed -i 's/\bCoverStop(/CoverStopImpl(/g' src/common/Cover.cpp
```

Verify by reading the file: the include should now point to `CoverStopImpl.hpp` and the init list should construct `CoverStopImpl`.

- [ ] **Step 6: Update `test/CoverMovementTest.cpp` to use `CoverStopImpl`**

```bash
sed -i 's|#include "common/CoverStop.hpp"|#include "common/CoverStopImpl.hpp"|' test/CoverMovementTest.cpp
sed -i 's/\bCoverStop stopper(/CoverStopImpl stopper(/g' test/CoverMovementTest.cpp
```

- [ ] **Step 7: Build and run all tests**

```bash
cd build && cmake && make -j$(nproc) && ./home_automation_test
```

Expected: build succeeds, all tests pass.

- [ ] **Step 8: Verify device build**

```bash
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
```

Expected: device build succeeds.

- [ ] **Step 9: Commit**

```bash
git add -A
git commit -m "Add abstract CoverStop base; rename concrete to CoverStopImpl"
```

---

## Task 3: Add `FakeCoverStop` to `CoverUpdateTest.cpp`

**Files:**
- Modify: `test/CoverUpdateTest.cpp`

- [ ] **Step 1: Add the `FakeCoverStop` class definition**

At the top of `test/CoverUpdateTest.cpp`, just after the `FakeCoverMovement` class definition, add:

```cpp
class FakeCoverStop : public CoverStop {
public:
    void stop() override {
        ++this->stopCount;
        this->triggered = true;
    }
    void reset() override {
        ++this->resetCount;
        this->triggered = false;
    }
    bool isTriggered() const override { return this->triggered; }
    bool isLatching() const override { return this->latching; }

    void setLatching(bool v) { this->latching = v; }
    int getStopCount() const { return this->stopCount; }
    int getResetCount() const { return this->resetCount; }

private:
    bool triggered = false;
    bool latching = false;
    int stopCount = 0;
    int resetCount = 0;
};
```

Add `#include "common/CoverStop.hpp"` at the top of the file (it may already be transitively included).

- [ ] **Step 2: Replace the real `CoverStop` with `FakeCoverStop` in the fixture**

In the `CoverUpdateTest` class:
- Change member type: `CoverStop stopper{...};` → `FakeCoverStop stopper;`
- Remove the `this->stopper.reset();` line that exists to undo the latching real `CoverStop`'s constructor side effect (no longer needed with a fake).

The fixture should end up looking like:

```cpp
class CoverUpdateTest : public EspTestBase {
public:
    static constexpr uint8_t stopPin = 50;
    CoverState ctx;
    FakeCoverMovement up;
    FakeCoverMovement down;
    FakeCoverStop stopper;
    CoverUpdate updateImpl{this->ctx, this->up, this->down, this->stopper};
    InterfaceConfig config;
    Actions actions{this->config};

    CoverUpdateTest() : ctx{/* field list unchanged */} {}
    // ... existing helper methods ...
};
```

- [ ] **Step 3: Build and run tests**

```bash
cd build && make -j$(nproc) && ./home_automation_test --gtest_filter=CoverUpdateTest.*
```

Expected: all `CoverUpdateTest` tests pass.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "CoverUpdateTest: use FakeCoverStop instead of real CoverStop"
```

---

## Task 4: Add `requestOpen()` to `CoverUpdate` (TDD)

**Files:**
- Modify: `test/CoverUpdateTest.cpp`
- Modify: `src/common/CoverUpdate.hpp`/`.cpp`

- [ ] **Step 1: Write the failing tests**

Add to `test/CoverUpdateTest.cpp`:

```cpp
TEST_F(CoverUpdateTest, RequestOpenStartsUpAndStopsDown) {
    this->updateImpl.requestOpen();
    EXPECT_EQ(this->up.startCount(), 1);
    EXPECT_EQ(this->down.stopCount(), 1);
    EXPECT_TRUE(this->ctx.stateChanged);
    EXPECT_EQ(this->ctx.targetPosition, -1);
}

TEST_F(CoverUpdateTest, RequestOpenIsIdempotentWhenUpAlreadyStarted) {
    this->up.setStarted(true);
    this->updateImpl.requestOpen();
    EXPECT_EQ(this->up.startCount(), 0);
    EXPECT_FALSE(this->ctx.stateChanged);
}
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
cd build && make -j$(nproc) && ./home_automation_test --gtest_filter=CoverUpdateTest.RequestOpen*
```

Expected: compile error (`requestOpen` is not a member of `CoverUpdate`).

- [ ] **Step 3: Add the declaration**

In `src/common/CoverUpdate.hpp`, add to the public section:

```cpp
    void requestOpen();
```

- [ ] **Step 4: Implement `requestOpen`**

In `src/common/CoverUpdate.cpp`, add:

```cpp
void CoverUpdate::requestOpen() {
    this->state.targetPosition = -1;
    if (!this->up.isStarted()) {
        this->down.stop();
        this->up.start();
        this->state.stateChanged = true;
    }
}
```

- [ ] **Step 5: Run tests to verify they pass**

```bash
make -j$(nproc) && ./home_automation_test --gtest_filter=CoverUpdateTest.RequestOpen*
```

Expected: both new tests pass.

- [ ] **Step 6: Run full test suite**

```bash
./home_automation_test
```

Expected: all tests pass.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "CoverUpdate: add requestOpen method"
```

---

## Task 5: Add `requestClose()` to `CoverUpdate` (TDD)

**Files:**
- Modify: `test/CoverUpdateTest.cpp`
- Modify: `src/common/CoverUpdate.hpp`/`.cpp`

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_F(CoverUpdateTest, RequestCloseStartsDownAndStopsUp) {
    this->updateImpl.requestClose();
    EXPECT_EQ(this->down.startCount(), 1);
    EXPECT_EQ(this->up.stopCount(), 1);
    EXPECT_TRUE(this->ctx.stateChanged);
    EXPECT_EQ(this->ctx.targetPosition, -1);
}

TEST_F(CoverUpdateTest, RequestCloseIsIdempotentWhenDownAlreadyStarted) {
    this->down.setStarted(true);
    this->updateImpl.requestClose();
    EXPECT_EQ(this->down.startCount(), 0);
    EXPECT_FALSE(this->ctx.stateChanged);
}
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
make -j$(nproc) && ./home_automation_test --gtest_filter=CoverUpdateTest.RequestClose*
```

Expected: compile error.

- [ ] **Step 3: Add the declaration**

In `src/common/CoverUpdate.hpp`:

```cpp
    void requestClose();
```

- [ ] **Step 4: Implement `requestClose`**

In `src/common/CoverUpdate.cpp`:

```cpp
void CoverUpdate::requestClose() {
    this->state.targetPosition = -1;
    if (!this->down.isStarted()) {
        this->up.stop();
        this->down.start();
        this->state.stateChanged = true;
    }
}
```

- [ ] **Step 5: Run tests**

```bash
make -j$(nproc) && ./home_automation_test --gtest_filter=CoverUpdateTest.RequestClose*
```

Expected: both new tests pass.

- [ ] **Step 6: Run full suite**

```bash
./home_automation_test
```

Expected: all tests pass.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "CoverUpdate: add requestClose method"
```

---

## Task 6: Add `requestStop()` to `CoverUpdate` (TDD)

**Files:**
- Modify: `test/CoverUpdateTest.cpp`
- Modify: `src/common/CoverUpdate.hpp`/`.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
TEST_F(CoverUpdateTest, RequestStopStopsAll) {
    this->updateImpl.requestStop();
    EXPECT_EQ(this->up.stopCount(), 1);
    EXPECT_EQ(this->down.stopCount(), 1);
    EXPECT_EQ(this->stopper.getStopCount(), 1);
    EXPECT_EQ(this->ctx.targetPosition, -1);
}
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
make -j$(nproc) && ./home_automation_test --gtest_filter=CoverUpdateTest.RequestStop*
```

Expected: compile error.

- [ ] **Step 3: Add the declaration**

In `src/common/CoverUpdate.hpp`:

```cpp
    void requestStop();
```

- [ ] **Step 4: Implement `requestStop`**

In `src/common/CoverUpdate.cpp`:

```cpp
void CoverUpdate::requestStop() {
    this->state.targetPosition = -1;
    this->up.stop();
    this->down.stop();
    this->stopper.stop();
}
```

- [ ] **Step 5: Run tests**

```bash
make -j$(nproc) && ./home_automation_test --gtest_filter=CoverUpdateTest.RequestStop*
```

Expected: test passes.

- [ ] **Step 6: Run full suite**

```bash
./home_automation_test
```

Expected: all tests pass.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "CoverUpdate: add requestStop method"
```

---

## Task 7: Add `requestSetPosition(int)` to `CoverUpdate` (TDD)

**Files:**
- Modify: `test/CoverUpdateTest.cpp`
- Modify: `src/common/CoverUpdate.hpp`/`.cpp`

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_F(CoverUpdateTest, RequestSetPositionRejectsOutOfRange) {
    this->ctx.position = 50;
    this->updateImpl.requestSetPosition(-1);
    EXPECT_EQ(this->ctx.targetPosition, -1);
    EXPECT_EQ(this->up.startCount(), 0);
    EXPECT_EQ(this->down.startCount(), 0);

    this->ctx.targetPosition = -1;
    this->updateImpl.requestSetPosition(101);
    EXPECT_EQ(this->ctx.targetPosition, -1);
    EXPECT_EQ(this->up.startCount(), 0);
    EXPECT_EQ(this->down.startCount(), 0);
}

TEST_F(CoverUpdateTest, RequestSetPositionGreaterThanCurrentOpens) {
    this->ctx.position = 30;
    this->updateImpl.requestSetPosition(70);
    EXPECT_EQ(this->ctx.targetPosition, 70);
    EXPECT_EQ(this->ctx.restartCount, 0u);
    EXPECT_EQ(this->up.startCount(), 1);
    EXPECT_EQ(this->down.stopCount(), 1);
}

TEST_F(CoverUpdateTest, RequestSetPositionLessThanCurrentCloses) {
    this->ctx.position = 70;
    this->updateImpl.requestSetPosition(30);
    EXPECT_EQ(this->ctx.targetPosition, 30);
    EXPECT_EQ(this->down.startCount(), 1);
    EXPECT_EQ(this->up.stopCount(), 1);
}

TEST_F(CoverUpdateTest, RequestSetPositionEqualToCurrentStops) {
    this->ctx.position = 50;
    this->updateImpl.requestSetPosition(50);
    EXPECT_EQ(this->ctx.targetPosition, 50);
    EXPECT_EQ(this->up.stopCount(), 1);
    EXPECT_EQ(this->down.stopCount(), 1);
    EXPECT_EQ(this->stopper.getStopCount(), 1);
}

TEST_F(CoverUpdateTest, RequestSetPositionAtBoundaryOpen) {
    this->ctx.position = 30;
    this->updateImpl.requestSetPosition(100);
    EXPECT_EQ(this->up.startCount(), 1);
}

TEST_F(CoverUpdateTest, RequestSetPositionAtBoundaryClosed) {
    this->ctx.position = 30;
    this->updateImpl.requestSetPosition(0);
    EXPECT_EQ(this->down.startCount(), 1);
}

TEST_F(CoverUpdateTest, RequestSetPositionResetsRestartCount) {
    this->ctx.position = 30;
    this->ctx.restartCount = 2;
    this->updateImpl.requestSetPosition(70);
    EXPECT_EQ(this->ctx.restartCount, 0u);
}
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
make -j$(nproc) && ./home_automation_test --gtest_filter=CoverUpdateTest.RequestSetPosition*
```

Expected: compile error.

- [ ] **Step 3: Add the declaration**

In `src/common/CoverUpdate.hpp`:

```cpp
    void requestSetPosition(int value);
```

- [ ] **Step 4: Implement `requestSetPosition`**

In `src/common/CoverUpdate.cpp`:

```cpp
void CoverUpdate::requestSetPosition(int value) {
    if (value < 0 || value > 100) {
        this->log("Position out of range: " + tools::intToString(value));
        return;
    }

    if (this->state.position == -1) {
        this->log("Position is not known, calibrating.");
    }

    this->state.targetPosition = value;
    this->state.restartCount = 0;

    if (value < this->state.position) {
        if (!this->down.isStarted()) {
            this->up.stop();
            this->down.start();
            this->state.stateChanged = true;
        }
    } else if (value > this->state.position) {
        if (!this->up.isStarted()) {
            this->down.stop();
            this->up.start();
            this->state.stateChanged = true;
        }
    } else {
        this->up.stop();
        this->down.stop();
        this->stopper.stop();
    }
}
```

**Note:** the spec originally called for delegation to `requestOpen()` / `requestClose()` / `requestStop()`. That would not work — those methods set `targetPosition = -1`, which would clobber the value this method just assigned and break the tests. The branches are intentionally inlined to preserve `targetPosition = value`. The idempotency guards (`!isStarted()`) match the behavior of the sibling methods.

Verify `#include "../tools/string.hpp"` is present at the top of `CoverUpdate.cpp` (it already is).

- [ ] **Step 5: Run tests**

```bash
make -j$(nproc) && ./home_automation_test --gtest_filter=CoverUpdateTest.RequestSetPosition*
```

Expected: all 7 new tests pass.

- [ ] **Step 6: Run full suite**

```bash
./home_automation_test
```

Expected: all tests pass.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "CoverUpdate: add requestSetPosition method"
```

---

## Task 8: Refactor `Cover::execute()` to use `updateImpl.request*` directly

**Files:**
- Modify: `src/common/Cover.cpp`

- [ ] **Step 1: Replace `Cover::execute` body**

In `src/common/Cover.cpp`, replace the current `Cover::execute` method with:

```cpp
void Cover::execute(const std::string& command) {
    if (command == "STOP") {
        this->updateImpl.requestStop();
    } else if (command == "OPEN") {
        this->updateImpl.requestOpen();
    } else if (command == "CLOSE") {
        this->updateImpl.requestClose();
    } else {
        auto pos = tools::fromString<int>(command);
        if (!pos.has_value()) {
            this->log("Invalid command: " + command);
            return;
        }
        this->updateImpl.requestSetPosition(*pos);
    }
}
```

- [ ] **Step 2: Build and run all tests**

```bash
make -j$(nproc) && ./home_automation_test
```

Expected: all tests pass.

- [ ] **Step 3: Verify device build**

```bash
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
```

Expected: device build succeeds.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "Cover::execute: route commands through updateImpl.request*"
```

---

## Task 9: Add `unique_ptr` ownership transfer to `CoverUpdate`

**Files:**
- Modify: `src/common/CoverUpdate.hpp`/`.cpp`
- Modify: `src/common/Cover.hpp`/`.cpp`
- Modify: `test/CoverUpdateTest.cpp`

This task has a subtle construction issue: `CoverMovementImpl`'s constructor takes `CoverStop& stopper` as its second parameter, but `CoverStopImpl` is supposed to be owned by `CoverUpdate` via `unique_ptr`. The `Cover` constructor must build the three components in the right order and then move them into `CoverUpdate`. To keep `CoverUpdate updateImpl;` as a direct member (not a `unique_ptr`), we use a small static helper function.

- [ ] **Step 1: Update `CoverUpdate` constructor to take `std::unique_ptr`**

In `src/common/CoverUpdate.hpp`:
- Add `#include <memory>` at the top.
- Change the constructor signature:

```cpp
    CoverUpdate(
        CoverState& state,
        std::unique_ptr<CoverMovement> up,
        std::unique_ptr<CoverMovement> down,
        std::unique_ptr<CoverStop> stopper);
```

- Change the private members from references to `unique_ptr`:

```cpp
private:
    void log(const std::string& msg);

    CoverState& state;
    std::unique_ptr<CoverMovement> up;
    std::unique_ptr<CoverMovement> down;
    std::unique_ptr<CoverStop> stopper;
```

- [ ] **Step 2: Update `CoverUpdate` constructor body**

In `src/common/CoverUpdate.cpp`, replace the constructor body with:

```cpp
CoverUpdate::CoverUpdate(
    CoverState& state,
    std::unique_ptr<CoverMovement> up,
    std::unique_ptr<CoverMovement> down,
    std::unique_ptr<CoverStop> stopper)
    : state(state)
    , up(std::move(up))
    , down(std::move(down))
    , stopper(std::move(stopper)) {}
```

- [ ] **Step 3: Add a private static factory method to `Cover`**

The factory builds all three components in the right order and returns a fully-constructed `CoverUpdate` by value.

In `src/common/Cover.hpp`, add to the private section (next to the other private members):

```cpp
    static CoverUpdate makeUpdateImpl(
        CoverState& state, EspApi& esp, uint8_t upMovementPin,
        uint8_t downMovementPin, uint8_t upPin, uint8_t downPin,
        uint8_t stopPin, bool latching, bool invertOutput,
        std::ostream& debug);
```

In `src/common/Cover.cpp`, add at the top of the file (in an anonymous namespace, before the `Cover` constructor):

```cpp
namespace {}  // empty; the helper goes here
```

Wait, add the actual helper in the anonymous namespace. After the existing `namespace { constexpr int noPosition = -1; ... }` block, add:

```cpp
namespace {
std::unique_ptr<CoverMovementImpl> makeMovement(
    CoverState& state, CoverStop& stopper, uint8_t inputPin, uint8_t outputPin,
    int endPosition, int direction, const std::string& name) {
    return std::make_unique<CoverMovementImpl>(
        state, stopper, inputPin, outputPin, endPosition, direction, name);
}
}  // namespace
```

Then add the static factory definition (after the `Cover` constructors, or anywhere in the file):

```cpp
CoverUpdate Cover::makeUpdateImpl(
    CoverState& state, EspApi& esp, uint8_t upMovementPin,
    uint8_t downMovementPin, uint8_t upPin, uint8_t downPin,
    uint8_t stopPin, bool latching, bool invertOutput,
    std::ostream& debug) {
    auto stopper = std::make_unique<CoverStopImpl>(
        esp, stopPin, latching, invertOutput, debug, state.debugPrefix);
    auto up = std::make_unique<CoverMovementImpl>(
        state, *stopper, upMovementPin, upPin, 100, 1, "up");
    auto down = std::make_unique<CoverMovementImpl>(
        state, *stopper, downMovementPin, downPin, 0, -1, "down");
    return CoverUpdate(
        state, std::move(up), std::move(down), std::move(stopper));
}
```

- [ ] **Step 4: Update `Cover` member declarations in `Cover.hpp`**

In `src/common/Cover.hpp`, remove the following private members:
- `std::ostream& debug;`
- `EspApi& esp;`
- `Rtc& rtc;`
- `const std::string debugPrefix;`
- `const bool invertOutput;`
- `CoverStop stopper;`
- `CoverMovementImpl up;`
- `CoverMovementImpl down;`

Remove these private methods:
- `void stop();`
- `void beginOpening();`
- `void beginClosing();`
- `void beginMoving(CoverMovement& direction, CoverMovement& reverse);`
- `void setPosition(int value);`

Final `Cover.hpp` private section should be:

```cpp
private:
    void log(const std::string& msg);

    static CoverUpdate makeUpdateImpl(
        CoverState& state, EspApi& esp, uint8_t upMovementPin,
        uint8_t downMovementPin, uint8_t upPin, uint8_t downPin,
        uint8_t stopPin, bool latching, bool invertOutput,
        std::ostream& debug);

    CoverState state;
    CoverUpdate updateImpl;
```

The constructor's parameters (`debug`, `esp`, `rtc`, etc.) are still used; they're stored as references inside `CoverState` instead of as direct `Cover` members.

- [ ] **Step 5: Update `Cover` constructor body**

In `src/common/Cover.cpp`, replace the constructor's member init list. The new init list:

```cpp
Cover::Cover(
    std::ostream& debug, EspApi& esp, Rtc& rtc, uint8_t upMovementPin,
    uint8_t downMovementPin, uint8_t upPin, uint8_t downPin,
    uint8_t stopPin, bool latching, bool invertInput, bool invertOutput,
    int closedPosition, std::vector<PositionSensor> positionSensors,
    bool invertPositionSensors)
    : state{
          -1,                          // position
          false,                       // stateChanged
          -1,                          // activePositionSensor
          -1,                          // previouslyActivePositionSensor
          0,                           // previousMovementDirection
          -1,                          // targetPosition
          0,                           // restartCount
          std::move(positionSensors),  // positionSensors
          invertInput,                 // invertInput
          invertOutput,                // invertOutput
          invertPositionSensors,       // invertPositionSensors
          closedPosition,              // closedPosition
          rtc.next(),                  // positionId
          esp,                         // esp
          rtc,                         // rtc
          debug,                       // debug
          "Cover " + tools::intToString(upPin) + "." +
              tools::intToString(downPin) + ": ",  // debugPrefix
      },
      updateImpl(makeUpdateImpl(
          this->state, this->state.esp, upMovementPin, downMovementPin, upPin,
          downPin, stopPin, latching, invertOutput, this->state.debug)) {
    // Post-construction body (validation, sort, log initial position)
    if (this->state.positionSensors.size() == 1) {
        this->state.debug
            << "Invalid position sensors: there should be zero or at least 2."
            << std::endl;
        this->state.positionSensors.clear();
    }

    std::sort(
        this->state.positionSensors.begin(),
        this->state.positionSensors.end(),
        [](const PositionSensor& lhs, const PositionSensor& rhs) {
            return lhs.position < rhs.position;
        });

    if (!this->state.positionSensors.empty() &&
        (this->state.positionSensors.front().position != 0 ||
         this->state.positionSensors.back().position != 100)) {
        this->state.debug
            << "Invalid position sensors: positions should go from 0 to 100."
            << std::endl;
        this->state.positionSensors.clear();
    }

    this->state.position = this->rtc.get(this->state.positionId) - 1;
    this->log("Initial position: " + tools::intToString(this->state.position));
}
```

**Note on `this->state.esp` in the init list:** since `state` is declared before `updateImpl` in `Cover.hpp`, and members are initialized in declaration order, `this->state` is fully constructed before `updateImpl` is initialized. The `this->state.esp` reference is therefore valid when the factory reads it.

- [ ] **Step 6: Update `Cover::start()`, `Cover::update()`, `Cover::log()`**

In `src/common/Cover.cpp`:

```cpp
void Cover::start() {
    this->state.stateChanged = true;
}

void Cover::update(Actions action) {
    this->updateImpl.update(action);
}

void Cover::log(const std::string& msg) {
    this->state.debug << this->state.debugPrefix << msg << std::endl;
}
```

- [ ] **Step 7: Update `test/CoverUpdateTest.cpp` fixture to use `unique_ptr`**

In `test/CoverUpdateTest.cpp`, change the `CoverUpdateTest` class:

```cpp
class CoverUpdateTest : public EspTestBase {
public:
    CoverState ctx;
    FakeCoverMovement* upPtr;
    FakeCoverMovement* downPtr;
    FakeCoverStop* stopperPtr;
    std::unique_ptr<CoverUpdate> update;

    CoverUpdateTest()
        : ctx{/* field list unchanged */} {
        auto up = std::make_unique<FakeCoverMovement>();
        auto down = std::make_unique<FakeCoverMovement>();
        auto stopper = std::make_unique<FakeCoverStop>();
        this->upPtr = up.get();
        this->downPtr = down.get();
        this->stopperPtr = stopper.get();
        this->update = std::make_unique<CoverUpdate>(
            this->ctx, std::move(up), std::move(down), std::move(stopper));
    }
};
```

Add `#include <memory>` at the top of the file if not already present.

Update existing tests to use the new pointers:
- `this->updateImpl` → `this->update` (then `->requestX()` becomes `.requestX()` via deref, or `this->update->requestX()`)
- `this->up.startCount()` → `this->upPtr->startCount()`
- `this->down.startCount()` → `this->downPtr->startCount()`
- `this->stopper.getStopCount()` → `this->stopperPtr->getStopCount()`
- `this->stopper.stop()` (in `UpdateResetsStopperWhenStopped`) → `this->stopperPtr->stop()`

The cleanest way to do the bulk renames:

```bash
sed -i 's/this->updateImpl\./(*this->update)./g' test/CoverUpdateTest.cpp
sed -i 's/this->up\.startCount/this->upPtr->startCount/g' test/CoverUpdateTest.cpp
sed -i 's/this->up\.stopCount/this->upPtr->stopCount/g' test/CoverUpdateTest.cpp
sed -i 's/this->up\.setStarted/this->upPtr->setStarted/g' test/CoverUpdateTest.cpp
sed -i 's/this->up\.setMoving/this->upPtr->setMoving/g' test/CoverUpdateTest.cpp
sed -i 's/this->up\.setUpdateReturn/this->upPtr->setUpdateReturn/g' test/CoverUpdateTest.cpp
sed -i 's/this->down\.startCount/this->downPtr->startCount/g' test/CoverUpdateTest.cpp
sed -i 's/this->down\.stopCount/this->downPtr->stopCount/g' test/CoverUpdateTest.cpp
sed -i 's/this->down\.setStarted/this->downPtr->setStarted/g' test/CoverUpdateTest.cpp
sed -i 's/this->down\.setMoving/this->downPtr->setMoving/g' test/CoverUpdateTest.cpp
sed -i 's/this->down\.setUpdateReturn/this->downPtr->setUpdateReturn/g' test/CoverUpdateTest.cpp
sed -i 's/this->stopper\.getStopCount/this->stopperPtr->getStopCount/g' test/CoverUpdateTest.cpp
sed -i 's/this->stopper\.getResetCount/this->stopperPtr->getResetCount/g' test/CoverUpdateTest.cpp
sed -i 's/this->stopper\.isTriggered/this->stopperPtr->isTriggered/g' test/CoverUpdateTest.cpp
sed -i 's/this->stopper\.isLatching/this->stopperPtr->isLatching/g' test/CoverUpdateTest.cpp
sed -i 's/this->stopper\.stop()/this->stopperPtr->stop()/g' test/CoverUpdateTest.cpp
sed -i 's/this->stopper\.reset()/this->stopperPtr->reset()/g' test/CoverUpdateTest.cpp
```

Review the diff to confirm all substitutions are correct. The `updateImpl.requestOpen()` and similar calls become `(*this->update).requestOpen()` — which is fine but ugly. If desired, change to `this->update->requestOpen()` for readability. The `(*this->update).` form is unambiguous and the sed handles it consistently.

- [ ] **Step 8: Build and run all tests**

```bash
cmake && make -j$(nproc) && ./home_automation_test
```

Expected: build succeeds, all tests pass.

- [ ] **Step 9: Verify device build**

```bash
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
```

Expected: device build succeeds.

- [ ] **Step 10: Commit**

```bash
git add -A
git commit -m "CoverUpdate: take unique_ptr ownership of components"
```

---

## Task 10: Cleanup dead code and redundant members; clang-format

**Files:**
- Modify: `src/common/Cover.hpp`
- Modify: `src/common/Cover.cpp`
- Run `clang-format` on all modified files

- [ ] **Step 1: Verify dead methods are already removed**

In `src/common/Cover.hpp`, confirm these private methods are NOT present:
- `void stop();`
- `void beginOpening();`
- `void beginClosing();`
- `void beginMoving(...);`
- `void setPosition(int);`

If any are still present, remove them.

- [ ] **Step 2: Verify redundant members are already removed**

In `src/common/Cover.hpp`, confirm these members are NOT present:
- `std::ostream& debug;`
- `EspApi& esp;`
- `Rtc& rtc;`
- `const std::string debugPrefix;`
- `const bool invertOutput;`
- `CoverStop stopper;`
- `CoverMovementImpl up;`
- `CoverMovementImpl down;`

(They should have been removed in Task 9's Step 4.) If any are still present, remove them.

- [ ] **Step 3: Run clang-format on modified files**

```bash
clang-format -i src/common/Cover.hpp src/common/Cover.cpp \
              src/common/CoverState.hpp \
              src/common/CoverStop.hpp src/common/CoverStopImpl.hpp src/common/CoverStopImpl.cpp \
              src/common/CoverMovementImpl.hpp src/common/CoverMovementImpl.cpp \
              src/common/CoverUpdate.hpp src/common/CoverUpdate.cpp \
              test/CoverMovementTest.cpp test/CoverUpdateTest.cpp
```

- [ ] **Step 4: Build and run all tests**

```bash
make -j$(nproc) && ./home_automation_test
```

Expected: all tests pass.

- [ ] **Step 5: Verify device build**

```bash
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
```

Expected: device build succeeds.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "Cover: remove dead code and redundant members; clang-format"
```

---

## Task 11: Final device build verification

**Files:** (none modified unless issues found)

- [ ] **Step 1: Run full test suite**

```bash
make -j$(nproc) && ./home_automation_test
```

Expected: all tests pass.

- [ ] **Step 2: Run device build**

```bash
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
```

Expected: device build succeeds.

- [ ] **Step 3: Verify the four pain points are addressed**

Run a final read-through. Confirm:
- `Cover` no longer has `up`, `down`, `stopper`, or redundant service members; it has only `state` and `updateImpl`.
- `Cover::execute` only calls `updateImpl.request*()`; no direct `up.start()`, `down.stop()`, `stopper.stop()`, or `state.*` mutations beyond `state.stateChanged` in `start()`.
- `CoverState` is owned by `Cover` (single owner).
- `CoverUpdate` is the only class that touches `up`/`down`/`stopper` (it owns them via `unique_ptr`).

- [ ] **Step 4: Commit any final changes**

```bash
git add -A
git diff --cached --quiet || git commit -m "Final cleanup"
```

If no changes, this is a no-op.

---

## Self-review

**Spec coverage:** Each of the four pain points from the spec is addressed by a specific task:
- God struct → Task 1 (rename, single owner).
- Cover reaches in → Task 8 (execute via request*).
- CoverUpdate knows too much → Task 9 (legitimate ownership).
- Cross-class mutation → Task 8 + Task 10.

Abstract `CoverStop` → Task 2. `unique_ptr` ownership → Task 9. `request*` methods → Tasks 4–7. `FakeCoverStop` → Task 3. Integration test unchanged → enforced by all "Run full suite" verifications. Final device build → Task 11.

**Placeholder scan:** No TBD, TODO, or vague requirements. All code blocks are complete.

**Type consistency:** `CoverState` (struct), `CoverStop` (abstract base), `CoverStopImpl` (concrete), `CoverUpdate` (not renamed to `Impl`) — names used consistently. `state.position`, `state.targetPosition`, `state.stateChanged`, `state.restartCount` match the struct fields. `up.isStarted()` matches the abstract interface.

**Subtle correctness:**
- `Cover` member declaration order: `state` before `updateImpl`. `state` is fully constructed before `updateImpl`'s init reads `this->state.esp`, etc. ✓
- `CoverMovementImpl` constructor takes `CoverStop&` as second arg. The factory in Task 9 builds `CoverStopImpl` first, then dereferences it (`*stopper`) when building the two `CoverMovementImpl`s. The `unique_ptr<CoverStop>` is moved into `CoverUpdate` last. ✓
- `requestSetPosition`'s `value <` / `value >` is equivalent to the original `value == 0 || value <` / `value == 100 || value >` because position is bounded `[0, 100]`. ✓
