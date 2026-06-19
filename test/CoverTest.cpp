#include <algorithm>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "InterfaceTestBase.hpp"
#include "common/Cover.hpp"

namespace {

using CoverObservation = std::pair<std::string, std::string>;
using CoverSequence = std::vector<CoverObservation>;

void addState(
    CoverSequence& out, const std::string& state,
    const std::string& value = "") {
    out.emplace_back(state, value);
}

void createSequence(
    CoverSequence& out, const std::string& state, int from, int to,
    int step = 1) {
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

std::string diff(const CoverSequence& actual, const CoverSequence& expected) {
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
               << e->first << "," << (e->second.empty() ? "_" : e->second)
               << ")\n";
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

#define GET_PARAM(name, n)               \
    auto name = std::get<n>(GetParam()); \
    std::cout << #name "=" << name << std::endl

enum Pin : uint8_t {
    UpOutput = 1,
    DownOutput,
    UpInput,
    DownInput,
    StopOutput,
    PositionSensorBegin,
};

struct TestPositionSensor {
    PositionSensor sensor;
    int min;
    int max;

    TestPositionSensor(int value, int min, int max, bool invert)
        : sensor{value, 0, invert}, min(min), max(max) {}
};

class CoverTest : public InterfaceTestBase {
public:
    const int maxPosition = 10000;
    bool latching = false;
    int position = 0;
    bool isWorking = true;
    unsigned long previousTime = 0;
    bool movingUp = false;
    bool movingDown = false;
    std::vector<TestPositionSensor> positionSensors;

    CoverTest() {}

    void init(
        bool isLatching, std::vector<TestPositionSensor> positionSensors_) {
        this->latching = isLatching;

        std::vector<PositionSensor> positionSensorInput;
        positionSensorInput.reserve(positionSensors_.size());
        for (size_t i = 0; i < positionSensors_.size(); ++i) {
            auto& ps = positionSensors_[i];
            ps.sensor.pin = PositionSensorBegin + i;
            positionSensorInput.push_back(ps.sensor);
        }
        this->positionSensors = std::move(positionSensors_);

        this->initInterface(
            "cover", std::make_unique<Cover>(
                         this->debug, this->esp, this->rtc, UpInput, DownInput,
                         UpOutput, DownOutput, StopOutput, isLatching, false,
                         false, 10, std::move(positionSensorInput), false));
        this->esp.delay(10);
    }

    std::vector<TestPositionSensor> getPositionSensors(bool hasPositionSensor) {
        if (!hasPositionSensor) {
            return {};
        }

        return {
            TestPositionSensor(0, 0, 0, false),
            TestPositionSensor(
                100, this->maxPosition, this->maxPosition, false),
        };
    }

    void reboot() {
        this->esp.restart(false);
        this->rtc.reset();
        this->esp.digitalWrite(UpOutput, 0);
        this->esp.digitalWrite(DownOutput, 0);
        this->previousTime = 0;
        this->movingUp = false;
        this->movingDown = false;
        this->init(this->latching, this->positionSensors);
    }

    bool isMoving(uint8_t pin, bool value) {
        bool isStarted = this->esp.digitalRead(pin) != 0;
        bool result = this->latching ? this->esp.digitalRead(StopOutput) == 0 &&
                                           (value || isStarted)
                                     : isStarted;
        return result;
    }

    bool isMovingUp() { return this->isMoving(UpOutput, this->movingUp); }

    bool isMovingDown() { return this->isMoving(DownOutput, this->movingDown); }

    void loop() {
        auto now = this->esp.millis();
        std::cout << "Loop begin, time=" << now << std::endl;
        int delta = now - this->previousTime;
        const bool upOn = this->esp.digitalRead(UpOutput) != 0;
        const bool downOn = this->esp.digitalRead(DownOutput) != 0;

        if (this->isWorking) {
            if (upOn && downOn) {
                ADD_FAILURE() << "Should not try to move in both directions.";
            }

            if (this->latching) {
                if (upOn) {
                    std::cout << "Start moving up" << std::endl;
                    this->movingUp = true;
                    this->movingDown = false;
                } else if (downOn) {
                    std::cout << "Start moving down" << std::endl;
                    this->movingUp = false;
                    this->movingDown = true;
                } else if (this->esp.digitalRead(StopOutput) != 0) {
                    std::cout << "Stop" << std::endl;
                    this->movingUp = false;
                    this->movingDown = false;
                }
            } else {
                this->movingUp = upOn;
                this->movingDown = downOn;
            }
        } else {
            this->movingUp = false;
            this->movingDown = false;
        }

        int newPosition = this->position;

        if (this->movingUp) {
            newPosition = std::min(this->maxPosition, this->position + delta);
        } else if (this->movingDown) {
            newPosition = std::max(0, this->position - delta);
        }

        const auto movedUp = newPosition > this->position;
        const auto movedDown = newPosition < this->position;

        if (!movedUp) {
            this->movingUp = false;
        }

        if (!movedDown) {
            this->movingDown = false;
        }

        for (const auto& sensor : this->positionSensors) {
            bool value = newPosition >= sensor.min && newPosition <= sensor.max;
            this->esp.digitalWrite(
                sensor.sensor.pin, sensor.sensor.invert ? !value : value);
        }

        std::cout << "upOn=" << upOn << " downOn=" << downOn
                  << " movingUp=" << this->movingUp
                  << " movingDown=" << this->movingDown
                  << " position=" << newPosition << " movedUp=" << movedUp
                  << " movedDown=" << movedDown << std::endl;

        this->esp.digitalWrite(UpInput, movedUp);
        this->esp.digitalWrite(DownInput, movedDown);

        this->position = newPosition;
        this->previousTime = now;

        this->updateInterface();

        std::cout << "Loop end" << std::endl;
    }

    void open() {
        std::cout << "Open" << std::endl;
        this->interface.interface->execute("OPEN");
    }

    void close() {
        std::cout << "Close" << std::endl;
        this->interface.interface->execute("CLOSE");
    }

    void stop() {
        std::cout << "Stop" << std::endl;
        this->interface.interface->execute("STOP");
    }

    void setPosition(int value) {
        this->interface.interface->execute(std::to_string(value));
    }

    void loopFor(
        unsigned long time, unsigned long delay,
        std::function<void(unsigned long delta, size_t round)> func) {
        auto beginTime = this->esp.millis();
        size_t round = 0;
        std::cout << "---- loopFor " << time << "----" << std::endl;
        this->delayUntil(beginTime + time, delay, [&]() {
            this->loop();
            auto time = this->esp.millis() - beginTime;
            ++round;
            SCOPED_TRACE(
                "round=" + std::to_string(round) +
                " time=" + std::to_string(time) +
                " position=" + std::to_string(this->position));
            ASSERT_NO_FATAL_FAILURE(func(time, round));
        });
        std::cout << "---- loopFor done ----" << std::endl;
    }

    CoverSequence loopFor2(unsigned long time, unsigned long delay) {
        auto beginTime = this->esp.millis();
        std::cout << "---- loopFor2 " << time << "----" << std::endl;

        auto getCurrent = [this]() -> std::optional<CoverObservation> {
            const auto& v = this->interface.storedValue;
            if (v.empty()) {
                return std::nullopt;
            }
            if (v.size() == 1) {
                return CoverObservation{v.back(), ""};
            }
            return CoverObservation{v[v.size() - 2], v.back()};
        };

        std::optional<CoverObservation> last = getCurrent();
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

    // Run the simulation for a fixed 30ms window to capture state transitions
    // after a command. The 30ms window is long enough to cover the cover's
    // 20ms debounce plus the first value emission, so a single observe() call
    // captures the full state transition (e.g., "OPENING" then "OPENING/1").
    CoverSequence observe() { return this->loopFor2(30, 1); }

    static bool isDebouncing(unsigned long time, size_t round) {
        return time <= 20 || round == 1;
    }

    static bool isStopDebouncing(
        unsigned long time, size_t round, unsigned long endTime,
        unsigned long delay) {
        // The cover is in the stop debounce window after reaching the end of
        // travel, because handleEndOfMovement is delayed by debounceTime
        // (20ms) plus however long until the next update() round.
        // round == 1 covers Calibrate transition loops where endTime=0 and
        // the first iteration is already in the debounce window.
        // time >= endTime covers the iteration where the end is first reached.
        return round == 1 ||
               (time >= endTime &&
                time <= endTime +
                            std::max(static_cast<unsigned long>(delay), 20UL));
    }

    void calibrateToPosition(int position, unsigned long delay) {
        this->setPosition(position);
        this->loopFor(41000, delay, [](unsigned long, size_t) {});
        ASSERT_NO_FAILURE();
        if (position <= 10) {
            EXPECT_EQ(this->getValue(0), "CLOSED");
        } else {
            EXPECT_EQ(this->getValue(0), "OPEN");
        }
        EXPECT_EQ(this->getValue(1), std::to_string(position));
        std::cout << "---- Calibration done ----" << std::endl;
    }
};

class HasPositionSensorFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<bool>> {};

class BasicFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<int, bool, bool>> {};

class CalibrateFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<int, bool, bool, int>> {};

class MultiplePositionSensorsFixture
    : public CoverTest,
      public ::testing::WithParamInterface<
          std::tuple<int, bool, bool, bool, bool>> {};

class StopMomentarilyWhileCalibratingFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<int, bool>> {};

namespace {
const bool hasPositionSensorValues[] = {false, true};
const int delays1[] = {10, 50, 100, 500};
const int delays2[] = {10, 50, 100};
const bool latchings[] = {false, true};
const int calibrateStartPositions[] = {0, 5000, 8000, 10000};
}  // namespace

TEST_P(HasPositionSensorFixture, NormalMode) {
    GET_PARAM(hasPositionSensor, 0);

    this->position = 5000;
    this->init(false, this->getPositionSensors(hasPositionSensor));
    this->loop();
    EXPECT_EQ(this->observe(), CoverSequence{});

    this->open();
    {
        SCOPED_TRACE("open at init");
        CoverSequence expected;
        addState(expected, "OPENING");
        addState(expected, "OPENING", "1");
        auto s = this->observe();
        EXPECT_EQ(s, expected) << diff(s, expected);
    }

    this->stop();
    {
        SCOPED_TRACE("stop after open");
        CoverSequence expected;
        addState(expected, "CLOSED", "1");
        auto s = this->observe();
        EXPECT_EQ(s, expected) << diff(s, expected);
    }

    this->close();
    {
        SCOPED_TRACE("close after stop");
        CoverSequence expected;
        addState(expected, "CLOSING", "1");
        addState(expected, "CLOSING", "99");
        auto s = this->observe();
        EXPECT_EQ(s, expected) << diff(s, expected);
    }

    this->stop();
    {
        SCOPED_TRACE("stop after close");
        CoverSequence expected;
        addState(expected, "OPEN", "99");
        auto s = this->observe();
        EXPECT_EQ(s, expected) << diff(s, expected);
    }

    this->open();
    {
        SCOPED_TRACE("open after stop");
        CoverSequence expected;
        addState(expected, "OPENING", "99");
        addState(expected, "OPENING", "1");
        auto s = this->observe();
        EXPECT_EQ(s, expected) << diff(s, expected);
    }

    this->close();
    {
        SCOPED_TRACE("close after open 1");
        CoverSequence expected;
        addState(expected, "CLOSING", "1");
        addState(expected, "CLOSING", "99");
        auto s = this->observe();
        EXPECT_EQ(s, expected) << diff(s, expected);
    }

    this->open();
    {
        SCOPED_TRACE("open after close");
        CoverSequence expected;
        addState(expected, "OPENING", "99");
        addState(expected, "OPENING", "1");
        auto s = this->observe();
        EXPECT_EQ(s, expected) << diff(s, expected);
    }

    this->close();
    {
        SCOPED_TRACE("close after open 2");
        CoverSequence expected;
        addState(expected, "CLOSING", "1");
        addState(expected, "CLOSING", "99");
        auto s = this->observe();
        EXPECT_EQ(s, expected) << diff(s, expected);
    }
}

INSTANTIATE_TEST_SUITE_P(
    CoverTestHasPositionSensor, HasPositionSensorFixture,
    testing::Combine(testing::ValuesIn(hasPositionSensorValues)));

TEST_P(HasPositionSensorFixture, LatchingMode) {
    GET_PARAM(hasPositionSensor, 0);

    this->position = 5000;
    this->init(true, this->getPositionSensors(hasPositionSensor));
    this->loop();
    EXPECT_EQ(this->observe(), CoverSequence{});

    this->open();
    {
        SCOPED_TRACE("open at init");
        CoverSequence expected;
        addState(expected, "OPENING");
        addState(expected, "OPENING", "1");
        auto s = this->observe();
        EXPECT_EQ(s, expected) << diff(s, expected);
    }

    this->stop();
    {
        SCOPED_TRACE("stop after open");
        CoverSequence expected;
        addState(expected, "CLOSED", "1");
        auto s = this->observe();
        EXPECT_EQ(s, expected) << diff(s, expected);
    }

    this->close();
    {
        SCOPED_TRACE("close after stop");
        CoverSequence expected;
        addState(expected, "CLOSING", "1");
        addState(expected, "CLOSING", "99");
        auto s = this->observe();
        EXPECT_EQ(s, expected) << diff(s, expected);
    }

    this->stop();
    {
        SCOPED_TRACE("stop after close");
        CoverSequence expected;
        addState(expected, "OPEN", "99");
        auto s = this->observe();
        EXPECT_EQ(s, expected) << diff(s, expected);
    }

    this->open();
    {
        SCOPED_TRACE("open after stop");
        CoverSequence expected;
        addState(expected, "OPENING", "99");
        addState(expected, "OPENING", "1");
        auto s = this->observe();
        EXPECT_EQ(s, expected) << diff(s, expected);
    }

    this->close();
    {
        SCOPED_TRACE("close after open 1");
        CoverSequence expected;
        addState(expected, "CLOSING", "1");
        addState(expected, "CLOSING", "99");
        auto s = this->observe();
        EXPECT_EQ(s, expected) << diff(s, expected);
    }

    this->open();
    {
        SCOPED_TRACE("open after close");
        CoverSequence expected;
        addState(expected, "OPENING", "99");
        addState(expected, "OPENING", "1");
        auto s = this->observe();
        EXPECT_EQ(s, expected) << diff(s, expected);
    }

    this->close();
    {
        SCOPED_TRACE("close after open 2");
        CoverSequence expected;
        addState(expected, "CLOSING", "1");
        addState(expected, "CLOSING", "99");
        auto s = this->observe();
        EXPECT_EQ(s, expected) << diff(s, expected);
    }
}

TEST_P(BasicFixture, Open) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->loop();

    this->open();
    auto actual =
        this->loopFor2(11000, delay);  // 10000ms travel + 1000ms buffer

    CoverSequence expected;
    if (hasPositionSensor) {
        // With position sensor, the cover starts at position 0, so the
        // first observed state change is already OPENING/1 (no empty-value
        // OPENING transition is emitted because position is known).
        addState(expected, "OPENING", "1");
    } else {
        addState(expected, "OPENING");
        addState(expected, "OPENING", "1");
    }
    if (hasPositionSensor) {
        addState(expected, "OPENING", "100");
    } else {
        addState(expected, "CLOSED", "1");
    }
    addState(expected, "OPEN", "100");
    EXPECT_EQ(actual, expected) << diff(actual, expected);
}

TEST_P(BasicFixture, OpenWhileFullyOpen) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->position = 10000;
    this->loop();

    this->open();
    auto actual = this->loopFor2(1100, delay);

    CoverSequence expected;
    if (!hasPositionSensor) {
        // Without position sensors, position is unknown until the start
        // timeout (1000ms) fires and the cover reports its end position.
        addState(expected, "OPEN", "100");
    }
    // With position sensors, the cover is already OPEN/100 and the open()
    // command is a no-op, so no state transitions are emitted.
    EXPECT_EQ(actual, expected) << diff(actual, expected);
}

TEST_P(BasicFixture, CloseWhileFullyClosed) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->position = 0;
    this->loop();

    this->close();
    auto actual = this->loopFor2(1100, delay);

    CoverSequence expected;
    if (!hasPositionSensor) {
        // Without position sensors, position is unknown until the start
        // timeout (1000ms) fires and the cover reports its end position.
        addState(expected, "CLOSED", "0");
    }
    // With position sensors, the cover is already CLOSED/0 and the close()
    // command is a no-op, so no state transitions are emitted.
    EXPECT_EQ(actual, expected) << diff(actual, expected);
}

TEST_P(BasicFixture, Close) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->position = 10000;
    this->loop();

    this->close();
    auto actual =
        this->loopFor2(11000, delay);  // 10000ms travel + 1000ms buffer

    CoverSequence expected;
    // Mirror of Open: cover starts fully open (position 10000), close() drives
    // it to 0. Without position sensors, the initial state-only CLOSING
    // transition is emitted because position is unknown until the start
    // timeout fires. With position sensors, the first observed change is
    // already CLOSING/99 (state and value change together).
    if (!hasPositionSensor) {
        addState(expected, "CLOSING");
    }
    addState(expected, "CLOSING", "99");
    if (hasPositionSensor) {
        addState(expected, "CLOSING", "0");
    } else {
        addState(expected, "OPEN", "99");
    }
    addState(expected, "CLOSED", "0");
    EXPECT_EQ(actual, expected) << diff(actual, expected);
}

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
    if (hasPositionSensor) {
        // With position sensor, the cover starts at a known position
        // (0), so the first state already includes the position.
        addState(expected1, "OPENING", "1");
    } else {
        // Without position sensor, the cover first emits OPENING with no
        // position, then OPENING with the interpolated position.
        addState(expected1, "OPENING");
        addState(expected1, "OPENING", "1");
    }
    EXPECT_EQ(phase1, expected1) << diff(phase1, expected1);

    // The stop transition is emitted by the manual loop() above (cover
    // reports state at position 1, the only known position since move
    // time is not yet calibrated). Phase 2 captures the steady state, so
    // no new transitions are observed.
    CoverSequence expected2;
    EXPECT_EQ(phase2, expected2) << diff(phase2, expected2);

    EXPECT_EQ(this->position, 2000);
    EXPECT_FALSE(this->isMovingUp());
    EXPECT_FALSE(this->isMovingDown());
}

TEST_P(BasicFixture, StopWhileClosing) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->position = 10000;
    this->loop();

    this->close();
    auto phase1 = this->loopFor2(2000, delay);

    this->stop();
    this->esp.delay(delay);
    this->loop();
    auto phase2 = this->loopFor2(delay * 3, delay);

    CoverSequence expected1;
    // Mirror of StopWhileOpening: closing from a known position
    // emits CLOSING with the interpolated value during the 2s
    // window.
    if (hasPositionSensor) {
        addState(expected1, "CLOSING", "99");
    } else {
        addState(expected1, "CLOSING");
        addState(expected1, "CLOSING", "99");
    }
    EXPECT_EQ(phase1, expected1) << diff(phase1, expected1);

    // The manual loop() between phases catches the settle transition.
    // Phase 2 captures the steady state, so no new transitions are
    // observed.
    CoverSequence expected2;
    EXPECT_EQ(phase2, expected2) << diff(phase2, expected2);

    EXPECT_EQ(this->position, 8000);
    EXPECT_FALSE(this->isMovingUp());
    EXPECT_FALSE(this->isMovingDown());
}

/**
 * Calibration phases:
 * - Phase 1: start from unknown position, fully open.
 * - After phase 1, neither open nor close time is known. (*)
 * - Phase 2: start from known fully open position, fully close.
 * - After phase 2, close time is known.
 * - Phase 3: start from known fully closed position, fully open. (*)
 * - After phase 3, both open and close times are known.
 * - Phase 4: close to correct position.
 *
 * (*) If the starting position is 0 and there are position sensors, open time
 * is known after phase 1, so phase 3 is skipped and phase 4 opens instead.
 */
TEST_P(CalibrateFixture, Calibrate) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);
    GET_PARAM(start, 3);

    if (delay == 500) {
        std::cout << "Cannot test with too large delay" << std::endl;
        GTEST_SKIP();
    }

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->position = start;
    this->loop();

    this->setPosition(40);
    auto actual = this->loopFor2(41000, delay);

    CoverSequence expected;
    if (!hasPositionSensor && start == this->maxPosition) {
        // Phase 1 skipped (already at top). Phase 2: close full.
        addState(expected, "OPEN", "100");
        addState(expected, "CLOSING", "100");
        addState(expected, "CLOSING", "99");
        addState(expected, "OPEN", "99");
        addState(expected, "CLOSED", "0");
        // Phase 3: open full.
        addState(expected, "OPENING", "0");
        addState(expected, "OPENING", "1");
        addState(expected, "CLOSED", "1");
        addState(expected, "OPEN", "100");
        // Phase 4: close to 40.
        addState(expected, "CLOSING", "100");
        createSequence(expected, "CLOSING", 99, 40, -1);
        addState(expected, "OPEN", "40");
    } else if (!hasPositionSensor) {
        // Phase 1: open full (start timeout fires; cover reports
        // OPEN,100 after stop at the end of up movement).
        addState(expected, "OPENING");
        addState(expected, "OPENING", "1");
        addState(expected, "CLOSED", "1");
        addState(expected, "OPEN", "100");
        // Phase 2: close full.
        addState(expected, "CLOSING", "100");
        addState(expected, "CLOSING", "99");
        addState(expected, "OPEN", "99");
        addState(expected, "CLOSED", "0");
        // Phase 3: open full.
        addState(expected, "OPENING", "0");
        addState(expected, "OPENING", "1");
        addState(expected, "CLOSED", "1");
        addState(expected, "OPEN", "100");
        // Phase 4: close to 40.
        addState(expected, "CLOSING", "100");
        createSequence(expected, "CLOSING", 99, 40, -1);
        addState(expected, "OPEN", "40");
    } else if (hasPositionSensor && start == 0) {
        // Phase 1: open full.
        addState(expected, "OPENING", "1");
        addState(expected, "OPENING", "100");
        addState(expected, "OPEN", "100");
        // Phase 2: close full.
        addState(expected, "CLOSING", "99");
        addState(expected, "CLOSING", "0");
        addState(expected, "CLOSED", "0");
        // Phase 4 (no Phase 3, calibration is done): open to 40.
        addState(expected, "OPENING", "0");
        createSequence(expected, "OPENING", 1, 40);
        addState(expected, "OPEN", "40");
    } else if (hasPositionSensor && start == this->maxPosition) {
        // Phase 1 skipped. Phase 2: close full.
        addState(expected, "CLOSING", "99");
        addState(expected, "CLOSING", "0");
        addState(expected, "CLOSED", "0");
        // Phase 3: open full.
        addState(expected, "OPENING", "1");
        addState(expected, "OPENING", "100");
        addState(expected, "OPEN", "100");
        // Phase 4: close to 40.
        addState(expected, "CLOSING", "100");
        createSequence(expected, "CLOSING", 99, 40, -1);
        addState(expected, "OPEN", "40");
    } else {
        // hasPositionSensor && start in (0, maxPosition)
        // Phase 1: open full.
        addState(expected, "OPENING");
        addState(expected, "OPENING", "1");
        addState(expected, "OPENING", "100");
        addState(expected, "OPEN", "100");
        // Phase 2: close full.
        addState(expected, "CLOSING", "99");
        addState(expected, "CLOSING", "0");
        addState(expected, "CLOSED", "0");
        // Phase 3: open full.
        addState(expected, "OPENING", "1");
        addState(expected, "OPENING", "100");
        addState(expected, "OPEN", "100");
        // Phase 4: close to 40.
        addState(expected, "CLOSING", "100");
        createSequence(expected, "CLOSING", 99, 40, -1);
        addState(expected, "OPEN", "40");
    }

    EXPECT_EQ(actual, expected) << diff(actual, expected);
    EXPECT_FALSE(this->isMovingUp());
    EXPECT_FALSE(this->isMovingDown());
}

INSTANTIATE_TEST_SUITE_P(
    CoverTestCalibrate, CalibrateFixture,
    testing::Combine(
        testing::ValuesIn(delays1), testing::ValuesIn(latchings),
        testing::ValuesIn(hasPositionSensorValues),
        testing::ValuesIn(calibrateStartPositions)));

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
    addState(expected, "OPENING", "60");
    if (hasPositionSensor) {
        createSequence(expected, "OPENING", 61, 100);
    } else {
        createSequence(expected, "OPENING", 61, 99);
        addState(expected, "OPEN", "99");
    }
    addState(expected, "OPEN", "100");
    EXPECT_EQ(actual, expected) << diff(actual, expected);
}

TEST_P(BasicFixture, CloseAfterCalibrate) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    if (delay == 500) {
        GTEST_SKIP() << "delay=500 not supported for this test";
    }

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    ASSERT_NO_FATAL_FAILURE(this->calibrateToPosition(60, delay));
    this->close();
    auto actual = this->loopFor2(6200, delay);

    CoverSequence expected;
    addState(expected, "CLOSING", "60");
    // The last CLOSING position depends on timing alignment between
    // the cover's interpolation and the test's position reaching 0.
    // The cover's moveTime is calibrated to ~9900ms (slightly less than
    // the test's 10000ms full-travel time), so the cover's interpolation
    // runs ahead of the test's position. With larger delays, this
    // divergence causes the test's position to reach 0 (or the position
    // sensor to fire) before the cover reports positions 1 and 2.
    int lastClosing;
    if (hasPositionSensor) {
        lastClosing = (delay == 100) ? 3 : (delay == 50) ? 2 : 1;
        createSequence(expected, "CLOSING", 59, lastClosing, -1);
        addState(expected, "CLOSING", "0");
    } else {
        lastClosing = (delay == 100) ? 2 : 1;
        createSequence(expected, "CLOSING", 59, lastClosing, -1);
        addState(expected, "CLOSED", "1");
    }
    addState(expected, "CLOSED", "0");
    EXPECT_EQ(actual, expected) << diff(actual, expected);
}

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
    addState(expected2, "CLOSING", "40");
    addState(expected2, "OPEN", "40");
    EXPECT_EQ(phase2, expected2) << diff(phase2, expected2);

    EXPECT_EQ(this->position, 4000 - delay);
    EXPECT_FALSE(this->isMovingUp());
    EXPECT_FALSE(this->isMovingDown());
}

TEST_P(MultiplePositionSensorsFixture, MultiplePositionSensors) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(invertClosed, 2);
    GET_PARAM(invertMiddle, 3);
    GET_PARAM(invertOpen, 4);

    this->init(
        isLatching, {
                        TestPositionSensor{0, 0, 200, invertClosed},
                        TestPositionSensor{50, 4800, 5200, invertMiddle},
                        TestPositionSensor{100, 9800, 10000, invertOpen},
                    });
    this->loop();

    EXPECT_FALSE(this->isMovingUp());
    EXPECT_FALSE(this->isMovingDown());
    EXPECT_EQ(this->getValue(0), "CLOSED");
    EXPECT_EQ(this->getValue(1), "0");

    std::cerr << "Phase 1: opening time is not known, only position sensors "
                 "are reported."
              << std::endl;

    this->open();
    auto phase1 = this->loopFor2(10000, delay);
    auto phase1Settle = this->loopFor2(delay + 20 + delay, delay);

    // Uncalibrated: the cover can only report the values of the three
    // position sensors (0, 50, 100) plus the value just past each sensor
    // (1, 51, 99 for closing direction).
    CoverSequence expected1;
    addState(expected1, "OPENING", "0");
    addState(expected1, "OPENING", "1");
    addState(expected1, "OPENING", "50");
    addState(expected1, "OPENING", "51");
    addState(expected1, "OPENING", "100");
    EXPECT_EQ(phase1, expected1) << diff(phase1, expected1);

    CoverSequence expected1Settle;
    addState(expected1Settle, "OPEN", "100");
    EXPECT_EQ(phase1Settle, expected1Settle)
        << diff(phase1Settle, expected1Settle);

    std::cerr << "Phase 2: closing time is not known, only position sensors "
                 "are reported."
              << std::endl;

    this->close();
    auto phase2 = this->loopFor2(10000, delay);
    auto phase2Settle = this->loopFor2(delay + 20 + delay, delay);

    CoverSequence expected2;
    addState(expected2, "CLOSING", "100");
    addState(expected2, "CLOSING", "99");
    addState(expected2, "CLOSING", "50");
    addState(expected2, "CLOSING", "49");
    addState(expected2, "CLOSING", "0");
    EXPECT_EQ(phase2, expected2) << diff(phase2, expected2);

    CoverSequence expected2Settle;
    addState(expected2Settle, "CLOSED", "0");
    EXPECT_EQ(phase2Settle, expected2Settle)
        << diff(phase2Settle, expected2Settle);

    std::cerr << "Phase 3: opening time is known, positions are interpolated "
                 "between position sensors."
              << std::endl;

    this->open();
    auto phase3 = this->loopFor2(10000, delay);
    auto phase3Settle = this->loopFor2(delay + 20 + delay, delay);

    // Calibrated: positions are interpolated linearly between sensors. With
    // delay=100, the cover advances by ~1.11 per tick, so every 10th value
    // starting at 9 is skipped (the integer position jumps by 2). For
    // delay=10 and delay=50, the tick resolution is fine enough that all
    // values 0..100 are observed.
    CoverSequence expected3;
    addState(expected3, "OPENING", "0");
    if (delay == 100) {
        for (int i = 1; i <= 100; ++i) {
            if (i % 10 != 9) {
                addState(expected3, "OPENING", std::to_string(i));
            }
        }
    } else {
        createSequence(expected3, "OPENING", 1, 100);
    }
    EXPECT_EQ(phase3, expected3) << diff(phase3, expected3);

    CoverSequence expected3Settle;
    addState(expected3Settle, "OPEN", "100");
    EXPECT_EQ(phase3Settle, expected3Settle)
        << diff(phase3Settle, expected3Settle);

    std::cerr << "Phase 4: closing time is known, positions are interpolated "
                 "between position sensors."
              << std::endl;

    this->close();
    auto phase4 = this->loopFor2(10000, delay);
    auto phase4Settle = this->loopFor2(delay + 20 + delay, delay);

    // Mirror of phase 3: with delay=100, the skipped values are
    // 91, 81, 71, ..., 11, 1 (the cover's integer position jumps by 2
    // going down). For delay=10 and delay=50, all values 100..0 are
    // observed.
    CoverSequence expected4;
    addState(expected4, "CLOSING", "100");
    if (delay == 100) {
        for (int i = 99; i >= 0; --i) {
            if (i % 10 != 1) {
                addState(expected4, "CLOSING", std::to_string(i));
            }
        }
    } else {
        createSequence(expected4, "CLOSING", 99, 0, -1);
    }
    EXPECT_EQ(phase4, expected4) << diff(phase4, expected4);

    CoverSequence expected4Settle;
    addState(expected4Settle, "CLOSED", "0");
    EXPECT_EQ(phase4Settle, expected4Settle)
        << diff(phase4Settle, expected4Settle);

    EXPECT_FALSE(this->isMovingUp());
    EXPECT_FALSE(this->isMovingDown());
}

INSTANTIATE_TEST_SUITE_P(
    CoverTestMultiplePositionSensors, MultiplePositionSensorsFixture,
    testing::Combine(
        testing::ValuesIn(delays2), testing::ValuesIn(latchings),
        testing::Values(false, true), testing::Values(false, true),
        testing::Values(false, true)));

TEST_P(BasicFixture, StopEarlyWhileCalibrating) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->loop();

    this->setPosition(50);

    auto openFunc = [&](unsigned long time, size_t round) {
        if (hasPositionSensor || !this->isDebouncing(time, round)) {
            EXPECT_EQ(this->getValue(1), "1");
        }
        EXPECT_TRUE(this->isMovingUp());
        EXPECT_EQ(this->getValue(0), "OPENING");
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(1000, delay, openFunc));
    ASSERT_NO_FAILURE();

    this->stop();
    this->esp.delay(delay);
    this->loop();

    auto checkNotMoving = [&](unsigned long time, size_t round) {
        if (!this->isStopDebouncing(time, round, 0, delay)) {
            EXPECT_FALSE(this->isMovingUp());
            EXPECT_FALSE(this->isMovingDown());
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(delay * 3, delay, checkNotMoving));
}

TEST_P(
    StopMomentarilyWhileCalibratingFixture, StopMomentarilyWhileCalibrating) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);

    this->init(isLatching, this->getPositionSensors(true));
    this->loop();

    this->setPosition(50);

    auto openFunc = [&](unsigned long time, size_t round) {
        if (!this->isDebouncing(time, round)) {
            EXPECT_EQ(this->getValue(1), "1");
        }
        EXPECT_TRUE(this->isMovingUp());
        EXPECT_EQ(this->getValue(0), "OPENING");
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(1000, delay, openFunc));
    ASSERT_NO_FAILURE();

    this->stop();
    this->esp.delay(delay);
    this->loop();

    auto checkNotMoving = [&](unsigned long time, size_t round) {
        if (!this->isStopDebouncing(time, round, 0, delay)) {
            EXPECT_FALSE(this->isMovingUp());
            EXPECT_FALSE(this->isMovingDown());
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(delay * 3, delay, checkNotMoving));
}

TEST_P(CalibrateFixture, CalibrationFailsIfMovementCannotStart) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);
    GET_PARAM(start, 3);

    if (hasPositionSensor && delay == 500) {
        std::cout << "Cannot test position sensor with too large delay"
                  << std::endl;
        GTEST_SKIP();
    }

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->position = start;
    this->loop();

    this->isWorking = false;
    this->setPosition(50);

    const unsigned long t1 = 1000 + delay;

    std::string constantPosition;
    bool hasConstantPosition = false;
    if (hasPositionSensor) {
        if (start == 0) {
            constantPosition = "0";
            hasConstantPosition = true;
        } else if (start == this->maxPosition) {
            constantPosition = "100";
            hasConstantPosition = true;
        }
    }

    auto check = [&](unsigned long time, size_t) {
        if (!hasPositionSensor) {
            bool upActive, downActive;
            if (time < t1) {
                upActive = true;
                downActive = false;
            } else if (time < 2 * t1) {
                upActive = false;
                downActive = true;
            } else if (time < 3 * t1) {
                upActive = true;
                downActive = false;
            } else if (time < 4 * t1) {
                upActive = false;
                downActive = true;
            } else {
                upActive = false;
                downActive = false;
            }
            EXPECT_EQ(this->isMovingUp(), upActive);
            EXPECT_EQ(this->isMovingDown(), downActive);
        }

        if (hasPositionSensor) {
            if (hasConstantPosition) {
                EXPECT_EQ(this->getValue(1), constantPosition);
            } else {
                EXPECT_EQ(this->interface.storedValue.size(), 1u);
            }
        } else {
            std::string expectedPos;
            if (time < t1) {
                EXPECT_EQ(this->interface.storedValue.size(), 1u);
                return;
            } else if (time < 2 * t1) {
                expectedPos = "100";
            } else if (time < 3 * t1) {
                expectedPos = "0";
            } else if (time < 4 * t1) {
                expectedPos = "100";
            } else {
                expectedPos = "0";
            }
            EXPECT_EQ(this->getValue(1), expectedPos);
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(5 * t1, delay, check));
}

INSTANTIATE_TEST_SUITE_P(
    CoverTestStopMomentarilyWhileCalibrating,
    StopMomentarilyWhileCalibratingFixture,
    testing::Combine(testing::ValuesIn(delays2), testing::ValuesIn(latchings)));

INSTANTIATE_TEST_SUITE_P(
    CoverTestBasic, BasicFixture,
    testing::Combine(
        testing::ValuesIn(delays1), testing::ValuesIn(latchings),
        testing::ValuesIn(hasPositionSensorValues)));
