# Cover State-Machine Refactor: CoverState + unique_ptr Ownership

## Goal

Address four coupling pain points in the existing `Cover` / `CoverMovementContext` / `CoverUpdate` design, without changing observable behavior:

1. **`CoverMovementContext` is a god struct** — 14 fields shared by reference across `Cover`, `CoverUpdate`, and `CoverMovementImpl`; everything depends on the same mutable bag.
2. **`Cover` reaches into its components** — `Cover::stop()` calls `up.stop()`, `down.stop()`, `stopper.stop()` directly; `Cover::execute()` mutates `context.targetPosition`, `context.restartCount` directly.
3. **`CoverUpdate` knows about everything** — holds refs to `up`, `down`, `stopper`, AND reaches into `context.positionSensors`, `context.esp`, `context.rtc`, `context.position`, `context.stateChanged`, `context.targetPosition`, etc.
4. **Cross-class mutation of shared state** — `Cover` and `CoverUpdate` both write to `context.position`, `context.stateChanged`, `context.targetPosition`.

The existing `test/CoverTest.cpp` (integration, ~30 tests) must continue to pass unchanged. `test/CoverMovementTest.cpp` and `test/CoverUpdateTest.cpp` (unit tests) require mechanical renames plus a small set of new unit tests for the new `request*` command surface.

---

## Architecture

### Before

```
Cover (271 lines, src/common/Cover.hpp/.cpp)
  ├── CoverMovementContext context     (god struct, shared by ref)
  ├── CoverStop stopper
  ├── CoverMovementImpl up
  ├── CoverMovementImpl down
  └── CoverUpdate updateImpl
       └── (refs context, up, down, stopper)
```

### After

```
Cover (~70 lines, src/common/Cover.hpp/.cpp)
  ├── CoverState state                  (single owner of runtime + config + services)
  └── CoverUpdate updateImpl
       ├── std::unique_ptr<CoverMovement> up        (abstract base; concrete = CoverMovementImpl)
       ├── std::unique_ptr<CoverMovement> down
       └── std::unique_ptr<CoverStop> stopper       (abstract base; concrete = CoverStopImpl)
```

`CoverUpdate` is the **only** object that touches the three components. `Cover` only ever calls `updateImpl.requestX()` or `updateImpl.update(action)`.

### Why this addresses all four pain points

| Pain point | How it's addressed |
|---|---|
| God struct | Renamed `CoverState`, owned by `Cover`; only `CoverUpdate` mutates it after initial setup |
| Cover reaches into components | All movement-decision logic moved into `CoverUpdate::requestX()`; `Cover::execute()` is a pure dispatcher |
| CoverUpdate knows everything | `CoverUpdate` becomes the legitimate coordinator; it now *owns* the components, so the knowledge is centralized rather than distributed |
| Cross-class mutation | `Cover` only writes to `state.stateChanged` (in `start()`) and constructs initial state. All other state writes are inside `CoverUpdate` |

---

## File layout

### New files
| File | Description |
|------|-------------|
| `src/common/CoverState.hpp` | `CoverState` struct (replaces `CoverMovementContext.hpp`) |
| `src/common/CoverStop.hpp` | Abstract `CoverStop` base (replaces the concrete `CoverStop.hpp` content) |
| `src/common/CoverStopImpl.hpp` | Concrete `CoverStopImpl` (split from old `CoverStop.hpp`) |
| `src/common/CoverStopImpl.cpp` | Concrete `CoverStopImpl` (renamed from old `CoverStop.cpp`) |

### Renamed files
| From | To |
|------|-----|
| `src/common/CoverMovementContext.hpp` | `src/common/CoverState.hpp` |
| `src/common/CoverStop.hpp` | `src/common/CoverStopImpl.hpp` |
| `src/common/CoverStop.cpp` | `src/common/CoverStopImpl.cpp` |

### Modified files
| File | Change |
|------|--------|
| `src/common/CoverMovementImpl.hpp`/`.cpp` | `CoverMovementContext&` → `CoverState&` in constructor; `CoverStop&` already abstract |
| `src/common/CoverUpdate.hpp`/`.cpp` | Add `requestOpen/Close/Stop/SetPosition`; constructor takes `std::unique_ptr` to abstract bases |
| `src/common/Cover.hpp`/`.cpp` | Remove 5 redundant service members; remove `beginOpening/beginClosing/beginMoving/setPosition/stop`; replace direct `up.start()` etc. with `updateImpl.requestX()` |
| `test/CoverMovementTest.cpp` | `CoverMovementContext` → `CoverState`; `CoverStop` → `CoverStopImpl` in construction |
| `test/CoverUpdateTest.cpp` | Use `FakeCoverStop` (no `FakeEspApi` for stopper); add 5–8 new tests for `request*` methods |

### Unchanged files
| File | Note |
|------|------|
| `test/CoverTest.cpp` | Integration test; must pass without changes |
| `CMakeLists.txt` | Already globs `src/common/*.cpp` and `test/*.cpp` |
| `src/common/CoverMovement.hpp` | Already an abstract base; no change needed |

---

## Module details

### 1. `CoverState` (replaces `CoverMovementContext`)

```cpp
struct CoverState {
    // Runtime mutable state (modified by CoverUpdate::update and the request* methods)
    int position = -1;
    int targetPosition = -1;
    int activePositionSensor = -1;
    int previouslyActivePositionSensor = -1;
    int previousMovementDirection = 0;
    unsigned restartCount = 0;
    bool stateChanged = false;

    // Immutable config (set once at construction)
    std::vector<PositionSensor> positionSensors;
    int closedPosition;
    unsigned positionId;
    bool invertInput;
    bool invertOutput;
    bool invertPositionSensors;

    // External services (owned by caller; lifetime outlives Cover)
    EspApi& esp;
    Rtc& rtc;
    std::ostream& debug;
    std::string debugPrefix;

    bool hasPositionSensors() const { return !this->positionSensors.empty(); }
};
```

`CoverState` is a plain aggregate-initialized struct — no business logic, no validation. Validation lives in `Cover`'s constructor.

**Why a single struct, not split into Runtime/Config/Services:** all three categories are passed together everywhere; splitting would mean threading three references through every call site with no real encapsulation benefit. The "god struct" smell comes from being **shared by reference** (everyone pokes at it). Once `Cover` no longer touches it and `CoverUpdate` is the single mutator, the field count is no longer the issue.

### 2. Abstract `CoverStop` base

```cpp
class CoverStop {
public:
    virtual ~CoverStop() = default;
    virtual void stop() = 0;
    virtual void reset() = 0;
    virtual bool isTriggered() const = 0;
    virtual bool isLatching() const = 0;
};
```

This enables `FakeCoverStop` in tests, which needs no `EspApi`.

### 3. Concrete `CoverStopImpl`

Same constructor and implementation as today's `CoverStop`, but inherits from `CoverStop`:

```cpp
class CoverStopImpl : public CoverStop {
public:
    CoverStopImpl(
        EspApi& esp, uint8_t pin, bool latching, bool invertOutput,
        std::ostream& debug, std::string debugPrefix);
    void stop() override;
    void reset() override;
    bool isTriggered() const override;
    bool isLatching() const override;
private:
    // ... existing private members unchanged ...
};
```

### 4. `CoverMovementImpl` (modified)

Constructor changes `CoverMovementContext&` to `CoverState&`. The `stopper` parameter type is `CoverStop&` (the abstract base — same name as before, different role). All other members unchanged.

```cpp
CoverMovementImpl(
    CoverState& state, CoverStop& stopper, uint8_t inputPin,
    uint8_t outputPin, int endPosition, int direction,
    std::string directionName);
```

### 5. `CoverUpdate` (expanded with new command surface)

```cpp
class CoverUpdate {
public:
    CoverUpdate(
        CoverState& state,
        std::unique_ptr<CoverMovement> up,
        std::unique_ptr<CoverMovement> down,
        std::unique_ptr<CoverStop> stopper);

    // Per-tick update (unchanged role)
    void update(Actions& action);

    // High-level command surface (new)
    void requestOpen();
    void requestClose();
    void requestStop();
    void requestSetPosition(int value);

private:
    void log(const std::string& msg);

    CoverState& state;
    std::unique_ptr<CoverMovement> up;
    std::unique_ptr<CoverMovement> down;
    std::unique_ptr<CoverStop> stopper;
};
```

**Method semantics:**

`requestOpen()`:
- Sets `state.targetPosition = -1`
- Stops `down`, starts `up` (if not already started)
- Sets `state.stateChanged = true`

`requestClose()`:
- Sets `state.targetPosition = -1`
- Stops `up`, starts `down` (if not already started)
- Sets `state.stateChanged = true`

`requestStop()`:
- Sets `state.targetPosition = -1`
- Calls `up.stop()`, `down.stop()`, `stopper.stop()`

`requestSetPosition(int value)`:
- Validates `0 <= value <= 100`; logs and returns on invalid input
- If `state.position == -1`, logs "Position is not known, calibrating."
- Sets `state.targetPosition = value`, `state.restartCount = 0`
- If `value < state.position`: calls `requestClose()`
- Else if `value > state.position`: calls `requestOpen()`
- Else: calls `requestStop()`

`update(Actions& action)` — unchanged from current behavior.

### 6. `Cover` (simplified)

```cpp
class Cover : public Interface {
public:
    Cover(
        std::ostream& debug, EspApi& esp, Rtc& rtc, uint8_t upMovementPin,
        uint8_t downMovementPin, uint8_t upPin, uint8_t downPin,
        uint8_t stopPin, bool latching, bool invertInput, bool invertOutput,
        int closedPosition, std::vector<PositionSensor> positionSensors,
        bool invertPositionSensors);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    void log(const std::string& msg);

    CoverState state;
    CoverUpdate updateImpl;
};
```

**Member declaration order:** `state` first (initialized before `updateImpl`), `updateImpl` second. At destruction, `updateImpl` is destroyed first (releasing its unique_ptrs), then `state`. References from components to `state` remain valid throughout.

**Method bodies:**

```cpp
void Cover::start() {
    this->state.stateChanged = true;
}

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

void Cover::update(Actions action) {
    this->updateImpl.update(action);
}

void Cover::log(const std::string& msg) {
    this->state.debug << this->state.debugPrefix << msg << std::endl;
}
```

**Constructor body** (after the member init list):
- Sort `state.positionSensors` by position
- Validate (drop if 1 sensor, or if first/last not 0/100)
- Set `state.position = rtc.get(state.positionId) - 1` (initial position from RTC)
- Log initial position

**Removed members from `Cover`:**
- `std::ostream& debug;`
- `EspApi& esp;`
- `Rtc& rtc;`
- `const std::string debugPrefix;`
- `const bool invertOutput;`
- `CoverMovementContext context;`
- `CoverStop stopper;`
- `CoverMovementImpl up;`
- `CoverMovementImpl down;`
- (Replaced by `CoverState state` + `CoverUpdate updateImpl`)

**Removed methods from `Cover`:**
- `void stop();`
- `void beginOpening();`
- `void beginClosing();`
- `void beginMoving(...);`
- `void setPosition(int);`

---

## Test strategy

### `test/CoverMovementTest.cpp` (~22 tests, mostly mechanical)

- `CoverMovementContext` → `CoverState` (rename only; field list and order unchanged)
- `CoverStop` → `CoverStopImpl` in construction (uses the concrete type)
- All test logic unchanged
- Fixture still uses `FakeEspApi` (real `CoverStopImpl` needs it)

### `test/CoverUpdateTest.cpp` (~25 existing + 5–8 new)

**Setup changes:**
- `CoverMovementContext` → `CoverState`
- Real `CoverStop` → `FakeCoverStop` (no `FakeEspApi` for the stopper)
- Components constructed in the fixture, **references saved before move**, then components moved into `CoverUpdate` via `std::unique_ptr`

**Test logic:** Most existing tests stay the same — they test `update(action)`. The fixture simplifies because the latching `CoverStop` constructor used to call `stop()`, which used to require a follow-up `reset()` — no longer needed.

**New test cases for the `request*` methods:**

`requestOpen()`:
- When `up` not started, calls `up.start()` and `down.stop()`, sets `state.stateChanged = true`
- When `up` already started, does not call `up.start()` again
- Sets `state.targetPosition = -1`

`requestClose()`:
- When `down` not started, calls `down.start()` and `up.stop()`, sets `state.stateChanged = true`
- When `down` already started, does not call `down.start()` again
- Sets `state.targetPosition = -1`

`requestStop()`:
- Calls `up.stop()`, `down.stop()`, `stopper.stop()`
- Sets `state.targetPosition = -1`

`requestSetPosition(int)`:
- Out-of-range values: logs and returns without changing state
- Value equal to current position: calls `requestStop()` (no movement)
- Value greater than current position: calls `requestOpen()`
- Value less than current position: calls `requestClose()`
- Sets `state.targetPosition = value` and `state.restartCount = 0`
- When `state.position == -1`: logs "calibrating" but still proceeds

### `test/CoverTest.cpp` (integration, ~30 tests)

**Unchanged.** This is the safety net. Every parameterized test case must continue to pass after each migration step.

### `FakeCoverStop` (new, in `test/CoverUpdateTest.cpp`)

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

### Test fixture pattern (no accessors needed)

The test fixture constructs the components, **saves references before moving ownership**, and uses those references for inspection:

```cpp
class CoverUpdateTest : public EspTestBase {
public:
    CoverState state;
    // Raw pointers / references to the fakes the fixture owns-by-raw-pointer
    FakeCoverMovement* upPtr;
    FakeCoverMovement* downPtr;
    FakeCoverStop* stopperPtr;
    // Owned by CoverUpdate
    std::unique_ptr<CoverUpdate> update;

    CoverUpdateTest()
        : state{/* ... */} {
        auto up = std::make_unique<FakeCoverMovement>();
        auto down = std::make_unique<FakeCoverMovement>();
        auto stopper = std::make_unique<FakeCoverStop>();
        this->upPtr = up.get();
        this->downPtr = down.get();
        this->stopperPtr = stopper.get();
        this->update = std::make_unique<CoverUpdate>(
            this->state, std::move(up), std::move(down), std::move(stopper));
    }
};

// In a test:
this->update->requestOpen();
EXPECT_EQ(this->upPtr->startCount(), 1);
EXPECT_EQ(this->downPtr->stopCount(), 1);
```

The references (raw pointers) remain valid because the underlying objects live inside `CoverUpdate`'s `unique_ptr`s for the test's lifetime. No `static_cast` or accessor needed; the fixture retains direct typed access to its fakes.
```

---

## Migration order

The refactor is structured as a series of steps, each leaving the code in a working state where all existing tests pass. CMake auto-globs `src/common/*.cpp` and `test/*.cpp`, so file additions and renames are picked up automatically.

### Step 1: Rename `CoverMovementContext` → `CoverState`
- Rename file `src/common/CoverMovementContext.hpp` → `CoverState.hpp`
- Rename class `CoverMovementContext` → `CoverState` (no field changes)
- Update all `#include` and reference sites (Cover.hpp, Cover.cpp, CoverMovementImpl.hpp, CoverUpdate.hpp, both test files)
- **Verify:** `cmake && make && ./build/home_automation_test` — all tests pass

### Step 2: Add abstract `CoverStop`; rename `CoverStop` → `CoverStopImpl`
- Create `src/common/CoverStop.hpp` with the abstract base
- Rename `src/common/CoverStop.hpp`/`.cpp` → `CoverStopImpl.hpp`/`.cpp`
- Rename class `CoverStop` → `CoverStopImpl`, add `: public CoverStop`
- Update `Cover.cpp` to construct `std::make_unique<CoverStopImpl>(...)`
- Update `CoverMovementTest.cpp` to use `CoverStopImpl` in its construction
- **Verify:** All tests pass

### Step 3: Add `request*` methods to `CoverUpdate`
- Add the four `requestOpen/Close/Stop/SetPosition` methods
- Implement them by porting the logic from `Cover::beginMoving/beginOpening/beginClosing/setPosition/stop`
- **Keep** the existing `Cover::beginOpening/Closing/etc` for now — make them thin wrappers that call `updateImpl.requestX()`
- **Verify:** All tests pass (behavior unchanged)

### Step 4: Refactor `Cover::execute()` to call `updateImpl.request*` directly
- Replace `this->beginOpening()` → `this->updateImpl.requestOpen()`
- Replace `this->beginClosing()` → `this->updateImpl.requestClose()`
- Replace `this->setPosition(*pos)` → `this->updateImpl.requestSetPosition(*pos)`
- Replace `this->stop()` → `this->updateImpl.requestStop()`
- **Verify:** All tests pass

### Step 5: Add new `CoverUpdate` unit tests
- Add the new test cases for `requestOpen/Close/Stop/SetPosition`
- Convert `CoverUpdateTest` fixture to use `FakeCoverStop`
- **Verify:** New tests pass; existing tests still pass

### Step 6: Add `unique_ptr` ownership transfer to `CoverUpdate`
- Change `CoverUpdate` constructor to take `std::unique_ptr<CoverMovement>`, `std::unique_ptr<CoverMovement>`, `std::unique_ptr<CoverStop>`
- Update `Cover` constructor to construct the components and move them in
- Update both test fixtures to: construct components, save raw pointers for inspection, move components into `CoverUpdate`
- **Verify:** All tests pass

### Step 7: Cleanup
- Remove the wrapper methods from `Cover` (the now-dead `beginOpening/beginClosing/beginMoving/setPosition/stop`)
- Remove the 5 redundant service members from `Cover`
- Run `clang-format` on all modified files
- **Verify:** All tests pass, final code review

### Step 8: Final build and review
- `arduino-cli compile --fqbn esp8266:esp8266:generic --verify` — confirm device build still works
- Final read-through of the spec to confirm all four pain points are addressed
- Hand off for code review

---

## Constraints

- C++17, no exceptions in device code
- Instance members prefixed with `this->`
- `EspApi` used for all hardware calls in testable code
- Abstract bases (`CoverMovement`, `CoverStop`) enable mocking in unit tests
- `CoverState` references from `CoverMovementImpl` point to members owned by `Cover` — must remain valid for `Cover`'s lifetime
- All new files must follow existing code formatting (clang-format)
- `std::unique_ptr` is header-only in the standard library on supported toolchains (libstdc++, libc++); this is acceptable for the device build
- The `CoverUpdate` rename to `CoverUpdateImpl` is **not** part of this refactor — only the four new methods and ownership change are. The file is still called `CoverUpdate.hpp`/`.cpp` (consistent with the current code; the abstract-vs-impl split was already made for `CoverMovement` only).

---

## Out of scope

- Direct unit tests of `Cover::execute()` command parsing. After this refactor, `Cover` is thin enough to test in isolation with a mock `CoverUpdate`, but adding that mocking is a separate piece of work.
- Splitting `CoverState` into separate `Runtime` / `Config` / `Services` structs. Single struct is sufficient given the centralization of mutation in `CoverUpdate`.
- Changes to `CoverMovementImpl`'s internal logic. Only its constructor parameter type changes.
- Behavior changes. The integration test (`test/CoverTest.cpp`) is the contract; it must pass without modification.
