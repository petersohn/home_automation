# Cover Refactor: Extract CoverMovement, CoverUpdate, Stop

## Goal

Split `Cover` (~540 lines) into independently unit-testable parts:
- `CoverMovement` — handles one direction (up/down) of movement logic
- `CoverStop` — handles latching stop output
- `CoverUpdate` — the coordination logic currently in `Cover::update()`
- `Cover` — reduced to wiring, config ownership, and `execute()` command handling

Each new class shall have its own unit tests. Existing `CoverTest.cpp` stays as integration test.

---

## Architecture

### Before

```
Cover (540 lines, src/common/Cover.hpp/.cpp)
  ├── inner class Movement (one direction: start/stop/update)
  ├── inner class Stop (latching stop output)
  └── coordination: update(), execute(), setPosition(), state naming, calibration
```

### After

```
Cover (~200 lines, src/common/Cover.hpp/.cpp)
  ├── owns: CoverMovementContext, Stop, CoverMovementImpl (up), CoverMovementImpl (down), CoverUpdateImpl
  ├── handles: constructor, start(), execute(), setPosition(), beginMoving, stop, log, setOutput
  └── delegates update() entirely to CoverUpdateImpl

CoverStop (src/common/CoverStop.hpp/.cpp) — extracted inner class
CoverMovement (src/common/CoverMovement.hpp) — abstract base
CoverMovementImpl (src/common/CoverMovementImpl.hpp/.cpp) — concrete
CoverUpdate (src/common/CoverUpdate.hpp) — abstract base
CoverUpdateImpl (src/common/CoverUpdateImpl.hpp/.cpp) — concrete
CoverMovementContext (src/common/CoverMovementContext.hpp) — shared mutable state struct
```

---

## New Modules

### 1. CoverMovementContext (`src/common/CoverMovementContext.hpp`)

Plain struct, header-only (no .cpp). Holds all mutable state and immutable config that both `CoverMovement` and `CoverUpdate` need to read/write, avoiding tight coupling through `Cover& parent`.

```cpp
struct CoverMovementContext {
    int& position;
    bool& stateChanged;
    int& activePositionSensor;
    int& previouslyActivePositionSensor;
    int& previousMovementDirection;

    const std::vector<PositionSensor>& positionSensors;
    bool invertInput;
    bool invertOutput;

    EspApi& esp;
    Rtc& rtc;
    std::ostream& debug;
    std::string debugPrefix;

    // Target position tracking (needed by both CoverUpdateImpl and Cover::execute/setPosition)
    int& targetPosition;
    unsigned& restartCount;

    bool hasPositionSensors() const;
};
```

### 2. CoverStop (`src/common/CoverStop.hpp`, `src/common/CoverStop.cpp`)

Extracted from current `Cover::Stop` inner class. Depends only on `EspApi&`, `std::ostream&`, and a debug prefix.

```cpp
class CoverStop {
public:
    CoverStop(EspApi& esp, uint8_t pin, bool latching, std::ostream& debug, std::string debugPrefix);
    void stop();
    void reset();
    bool isTriggered() const;
    bool isLatching() const;
private:
    EspApi& esp;
    const uint8_t pin;
    const bool latching;
    bool triggered = false;
    std::ostream& debug;
    std::string debugPrefix;
};
```

### 3. CoverMovement (`src/common/CoverMovement.hpp`)

Abstract base class. Exposes only the interface needed by `CoverUpdate` and `Cover`.

```cpp
class CoverMovement {
public:
    virtual ~CoverMovement() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isMoving() const = 0;
    virtual bool isStarted() const = 0;
    virtual int update() = 0;
};
```

### 4. CoverMovementImpl (`src/common/CoverMovementImpl.hpp`, `src/common/CoverMovementImpl.cpp`)

Concrete implementation. Extracted from current `Cover::Movement` inner class. All `parent.x` references replaced by `this->context.x`.

Constructor:
```cpp
CoverMovementImpl(
    CoverMovementContext& context, CoverStop& stopper,
    uint8_t inputPin, uint8_t outputPin, int endPosition,
    int direction, std::string directionName);
```

### 5. CoverUpdate (`src/common/CoverUpdate.hpp`)

Abstract base class.

```cpp
class CoverUpdate {
public:
    virtual ~CoverUpdate() = default;
    virtual void update(Actions& action) = 0;
};
```

### 6. CoverUpdateImpl (`src/common/CoverUpdateImpl.hpp`, `src/common/CoverUpdateImpl.cpp`)

Concrete implementation. Contains the logic currently in `Cover::update()`:

- Read position sensors and update `activePositionSensor`/`previouslyActivePositionSensor`
- Call `up.update()` and `down.update()`, resolve new position
- Determine movement direction, set `stateChanged`
- Apply position-sensor-derived position overrides
- Handle stopper triggered state
- Fire Actions with state name and position
- Handle target-position calibration/restart logic

Constructor:
```cpp
CoverUpdateImpl(
    CoverMovementContext& context,
    CoverMovement& up, CoverMovement& down,
    CoverStop& stopper,
    int closedPosition);
```

`targetPosition` and `restartCount` are accessed via `context.targetPosition` and `context.restartCount`.

### 7. Cover (modified, simplified)

After refactoring, `Cover`:
- Owns `CoverMovementContext`'s underlying value members (position, stateChanged, activePositionSensor, previouslyActivePositionSensor, previousMovementDirection, targetPosition, restartCount) and passes references via the context struct
- Owns concrete instances: `CoverStop`, `CoverMovementImpl up`, `CoverMovementImpl down`, `CoverUpdateImpl`
- Owns pin config, invert flags, closedPosition
- `update()` delegates to `CoverUpdateImpl::update()`
- `execute()`/`setPosition()`/`beginMoving()`/`stop()`/`log()`/`setOutput()` remain in `Cover`

---

## File Changes

### New files
| File | Description |
|------|-------------|
| `src/common/CoverMovementContext.hpp` | Shared state struct (header-only) |
| `src/common/CoverStop.hpp` | CoverStop class |
| `src/common/CoverStop.cpp` | CoverStop implementation |
| `src/common/CoverMovement.hpp` | Abstract base class |
| `src/common/CoverMovementImpl.hpp` | Concrete implementation |
| `src/common/CoverMovementImpl.cpp` | Concrete implementation |
| `src/common/CoverUpdate.hpp` | Abstract base class |
| `src/common/CoverUpdateImpl.hpp` | Concrete implementation |
| `src/common/CoverUpdateImpl.cpp` | Concrete implementation |
| `test/CoverMovementTest.cpp` | Unit tests for CoverMovementImpl |
| `test/CoverUpdateTest.cpp` | Unit tests for CoverUpdateImpl |

### Modified files
| File | Change |
|------|--------|
| `src/common/Cover.hpp` | Remove inner classes Movement and Stop; add forward declarations; hold new component instances |
| `src/common/Cover.cpp` | Remove Movement/Stop implementations; simplify constructor; delegate update() |
| `CMakeLists.txt` | Already globs `src/common/*.cpp` and `test/*.cpp` — no changes needed |

### Unchanged files
| File | Note |
|------|------|
| `test/CoverTest.cpp` | Stays as integration test |

---

## Test Strategy

### CoverMovementImpl tests (`test/CoverMovementTest.cpp`)

Uses `FakeEspApi` and `FakeRtc` directly (no `InterfaceTestBase` needed).

Test scenarios:
- **start()**: Verifies output pin set, moving detection begins, stopper.reset() called
- **stop()**: Verifies output pin cleared, start reset
- **isMoving()** / **isStarted()**: Verify state queries
- **update() — moving without position sensors**: Position interpolates over time using calibrated move times
- **update() — moving with position sensors**: Position uses sensor positions, calculates move times
- **update() — reaching end position**: Sets end position, calculates move time, calls handleStopped
- **update() — start timeout**: Did not start moving within startTimeout → handleStopped
- **update() — stopper latching interaction**: stopper.isLatching() behavior
- **Move time calculation**: `calculateMoveTimeIfNeeded` / `calculateBeginAndEndPosition`

### CoverUpdateImpl tests (`test/CoverUpdateTest.cpp`)

Uses mock `CoverMovement` and `CoverStop` plus `FakeEspApi`/`FakeRtc`.

Test scenarios:
- **Position sensor reading**: Correctly sets `activePositionSensor` from digital reads
- **Position resolution**: When both movements disagree → position = -1, stop called
- **Movement direction detection**: Correctly detects OPENING/CLOSING/stopped
- **State emission**: Fires Actions with correct state name and position
- **Closed position threshold**: Position <= closedPosition reports "CLOSED", otherwise "OPEN"
- **Stopper trigger + not moving**: stopper.reset() called
- **Target position reached**: Sets targetPosition = -1, calls stop
- **Target position restart**: Re-initiates movement up to 3 times (restartCount)
- **Calibration flow**: Handles unknown position → opens/closes to calibrate → sets target

### Integration tests (`test/CoverTest.cpp`)

Unchanged — continues to test the full `Cover` component end-to-end.

---

## Constraints

- C++17, no exceptions in device code
- Instance members prefixed with `this->`
- `EspApi` used for all hardware calls in testable code
- `CoverMovement` and `CoverUpdate` abstract bases enable mocking in unit tests
- `CoverMovementContext` references point to members owned by `Cover` — must remain valid for `Cover`'s lifetime
- All new files must follow existing code formatting (clang-format)
