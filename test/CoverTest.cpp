#include <algorithm>
#include <iostream>
#include <tuple>

#include "InterfaceTestBase.hpp"
#include "common/Cover.hpp"

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

    static bool isDebouncing(unsigned long time, size_t round) {
        return time <= 20 || round == 1;
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

class NormalModeFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<bool>> {};

class LatchingModeFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<bool>> {};

class OpenFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<int, bool, bool>> {};

class CloseFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<int, bool, bool>> {};

class StopWhileOpeningFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<int, bool, bool>> {};

class StopWhileClosingFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<int, bool, bool>> {};

class CalibrateFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<int, bool, bool, int>> {};

class OpenAfterCalibrateFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<int, bool, bool>> {};

class CloseAfterCalibrateFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<int, bool, bool>> {};

class RestartAfterCalibrateFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<int, bool, bool>> {};

class MultiplePositionSensorsFixture
    : public CoverTest,
      public ::testing::WithParamInterface<
          std::tuple<int, bool, bool, bool, bool>> {};

namespace {
const bool hasPositionSensorValues[] = {false, true};
const int delays1[] = {10, 50, 100, 500};
const int delays2[] = {10, 50, 100};
const bool latchings[] = {false, true};
const int calibrateStartPositions[] = {0, 5000, 8000, 10000};
}  // namespace

TEST_P(NormalModeFixture, NormalMode) {
    GET_PARAM(hasPositionSensor, 0);
    auto check = [this](const std::string& name, int upValue, int downValue) {
        SCOPED_TRACE(name);
        EXPECT_EQ(this->esp.digitalRead(UpOutput), upValue);
        EXPECT_EQ(this->esp.digitalRead(DownOutput), downValue);
        this->esp.delay(10);
        this->loop();
        EXPECT_EQ(this->esp.digitalRead(UpOutput), upValue);
        EXPECT_EQ(this->esp.digitalRead(DownOutput), downValue);
    };

    this->position = 5000;
    this->init(false, this->getPositionSensors(hasPositionSensor));
    check("initial state", 0, 0);

    this->open();
    check("open at init", 1, 0);

    this->stop();
    check("stop after open", 0, 0);

    this->close();
    check("close after stop", 0, 1);

    this->stop();
    check("stop after close", 0, 0);

    this->open();
    check("open after stop", 1, 0);

    this->close();
    check("close after open 1", 0, 1);

    this->open();
    check("open after close", 1, 0);

    this->close();
    check("close after open 2", 0, 1);
}

INSTANTIATE_TEST_SUITE_P(
    NormalMode, NormalModeFixture,
    testing::Combine(testing::ValuesIn(hasPositionSensorValues)));

TEST_P(LatchingModeFixture, LatchingMode) {
    GET_PARAM(hasPositionSensor, 0);
    auto check = [this](
                     const std::string& name, int upValue, int downValue,
                     int stopValue) {
        SCOPED_TRACE(name);
        EXPECT_EQ(this->esp.digitalRead(UpOutput), upValue);
        EXPECT_EQ(this->esp.digitalRead(DownOutput), downValue);
        EXPECT_EQ(this->esp.digitalRead(StopOutput), stopValue);
        this->esp.delay(10);
        this->loop();
        EXPECT_EQ(this->esp.digitalRead(UpOutput), 0);
        EXPECT_EQ(this->esp.digitalRead(DownOutput), 0);
        EXPECT_EQ(this->esp.digitalRead(StopOutput), 0);
    };

    this->position = 5000;
    this->init(true, this->getPositionSensors(hasPositionSensor));
    check("initial state", 0, 0, 1);

    this->open();
    check("open at init", 1, 0, 0);

    this->stop();
    check("stop after open", 0, 0, 1);

    this->close();
    check("close after stop", 0, 1, 0);

    this->stop();
    check("stop after close", 0, 0, 1);

    this->open();
    check("open after stop", 1, 0, 0);

    this->close();
    check("close after open 1", 0, 1, 0);

    this->open();
    check("open after close", 1, 0, 0);

    this->close();
    check("close after open 2", 0, 1, 0);
}

INSTANTIATE_TEST_SUITE_P(
    LatchingMode, LatchingModeFixture,
    testing::Combine(testing::ValuesIn(hasPositionSensorValues)));

TEST_P(OpenFixture, Open) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->loop();

    this->open();
    auto func = [&](unsigned long time, size_t round) {
        if (!hasPositionSensor && (this->isDebouncing(time, round))) {
            EXPECT_TRUE(this->isMovingUp());
            EXPECT_EQ(this->interface.storedValue.size(), 1u);
            EXPECT_EQ(this->getValue(0), "OPENING");
        } else if (time <= 10000) {
            EXPECT_TRUE(this->isMovingUp());
            EXPECT_EQ(this->getValue(0), "OPENING");
            if (hasPositionSensor && time == 10000) {
                EXPECT_EQ(this->getValue(1), "100");
            } else {
                EXPECT_EQ(this->getValue(1), "1");
            }
        } else {
            EXPECT_TRUE(!this->isMovingUp());
            EXPECT_EQ(this->getValue(0), "OPEN");
            EXPECT_EQ(this->getValue(1), "100");
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(10100, delay, func));
}

INSTANTIATE_TEST_SUITE_P(
    Open, OpenFixture,
    testing::Combine(
        testing::ValuesIn(delays1), testing::ValuesIn(latchings),
        testing::ValuesIn(hasPositionSensorValues)));

class OpenWhileFullyOpenFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<int, bool, bool>> {};

TEST_P(OpenWhileFullyOpenFixture, OpenWhileFullyOpen) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->position = 10000;
    this->loop();

    this->open();

    auto func = [&](unsigned long time, size_t /*round*/) {
        if (time <= 1000) {
            EXPECT_EQ(this->esp.digitalRead(UpOutput), 1);
            EXPECT_EQ(this->position, 10000);
        } else {
            EXPECT_EQ(this->esp.digitalRead(UpOutput), 0);
            EXPECT_EQ(this->getValue(1), "100");
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(1100, delay, func));
}

INSTANTIATE_TEST_SUITE_P(
    OpenWhileFullyOpen, OpenWhileFullyOpenFixture,
    testing::Combine(
        testing::ValuesIn(delays1), testing::ValuesIn(latchings),
        testing::ValuesIn(hasPositionSensorValues)));

TEST_P(CloseFixture, Close) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->position = 10000;
    this->loop();

    this->close();
    auto func = [&](unsigned long time, size_t round) {
        if (!hasPositionSensor && (this->isDebouncing(time, round))) {
            EXPECT_TRUE(this->isMovingDown());
            EXPECT_EQ(this->interface.storedValue.size(), 1u);
            EXPECT_EQ(this->getValue(0), "CLOSING");
        } else if (time <= 10000) {
            EXPECT_TRUE(this->isMovingDown());
            EXPECT_EQ(this->getValue(0), "CLOSING");
            if (hasPositionSensor && time == 10000) {
                EXPECT_EQ(this->getValue(1), "0");
            } else {
                EXPECT_EQ(this->getValue(1), "99");
            }
        } else {
            EXPECT_TRUE(!this->isMovingUp());
            EXPECT_EQ(this->getValue(0), "CLOSED");
            EXPECT_EQ(this->getValue(1), "0");
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(10100, delay, func));
}

INSTANTIATE_TEST_SUITE_P(
    Close, CloseFixture,
    testing::Combine(
        testing::ValuesIn(delays1), testing::ValuesIn(latchings),
        testing::ValuesIn(hasPositionSensorValues)));

TEST_P(StopWhileOpeningFixture, StopWhileOpening) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->loop();

    this->open();
    ASSERT_NO_FATAL_FAILURE(
        this->loopFor(2000, delay, [](unsigned long, size_t) {}));
    ASSERT_NO_FAILURE();

    this->stop();
    this->esp.delay(delay);
    this->loop();

    EXPECT_FALSE(this->isMovingUp());
    EXPECT_FALSE(this->isMovingDown());
    EXPECT_EQ(this->position, 2000);
}

INSTANTIATE_TEST_SUITE_P(
    StopWhileOpening, StopWhileOpeningFixture,
    testing::Combine(
        testing::ValuesIn(delays1), testing::ValuesIn(latchings),
        testing::ValuesIn(hasPositionSensorValues)));

TEST_P(StopWhileClosingFixture, StopWhileClosing) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->position = 10000;
    this->loop();

    this->close();
    ASSERT_NO_FATAL_FAILURE(
        this->loopFor(2000, delay, [](unsigned long, size_t) {}));
    ASSERT_NO_FAILURE();

    this->stop();
    this->esp.delay(delay);
    this->loop();

    EXPECT_FALSE(this->isMovingUp());
    EXPECT_FALSE(this->isMovingDown());
    EXPECT_EQ(this->position, 8000);
}

INSTANTIATE_TEST_SUITE_P(
    StopWhileClosing, StopWhileClosingFixture,
    testing::Combine(
        testing::ValuesIn(delays1), testing::ValuesIn(latchings),
        testing::ValuesIn(hasPositionSensorValues)));

TEST_P(CalibrateFixture, Calibrate) {
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

    this->setPosition(40);

    std::cout << "Phase 1: open fully" << std::endl;
    if (start == this->maxPosition) {
        if (!hasPositionSensor) {
            std::cout
                << "Trying to open, but won't start because we are at the top."
                << std::endl;
            auto func1 = [&](unsigned long /*time*/, size_t /*round*/) {
                EXPECT_TRUE(this->isMovingUp());
                EXPECT_EQ(this->position, this->maxPosition);
                EXPECT_EQ(this->interface.storedValue.size(), 1u);
            };
            ASSERT_NO_FATAL_FAILURE(this->loopFor(1000, delay, func1));
            ASSERT_NO_FAILURE();
        } else {
            EXPECT_EQ(this->getValue(0), "OPEN");
            EXPECT_EQ(this->getValue(1), "100");
        }
    } else {
        std::cout << "Opening" << std::endl;
        const unsigned long travelTime = 10000 - start;
        auto func1 = [&](unsigned long time, size_t round) {
            EXPECT_TRUE(this->isMovingUp());
            EXPECT_EQ(this->getValue(0), "OPENING");
            if (!(hasPositionSensor && start == 0) &&
                (this->isDebouncing(time, round))) {
                EXPECT_EQ(this->interface.storedValue.size(), 1u);
            } else {
                if (hasPositionSensor && time == travelTime) {
                    EXPECT_EQ(this->getValue(1), "100");
                } else {
                    EXPECT_EQ(this->getValue(1), "1");
                }
            }
        };
        ASSERT_NO_FATAL_FAILURE(this->loopFor(travelTime, delay, func1));
        ASSERT_NO_FAILURE();
    }

    std::cout << "Phase 2: move from fully open to fully closed, calculating "
                 "closing time."
              << std::endl;

    if (!(hasPositionSensor && start == this->maxPosition)) {
        auto funcOpen = [&](unsigned long, size_t) {
            EXPECT_TRUE(this->isMovingDown());
            EXPECT_EQ(this->getValue(0), "OPEN");
            EXPECT_EQ(this->getValue(1), "100");
        };

        ASSERT_NO_FATAL_FAILURE(this->loopFor(delay, delay, funcOpen));
        ASSERT_NO_FAILURE();
    }

    auto func2 = [&](unsigned long time, size_t round) {
        EXPECT_TRUE(this->isMovingDown());
        EXPECT_EQ(this->getValue(0), "CLOSING");
        if (!hasPositionSensor && (this->isDebouncing(time, round))) {
            EXPECT_EQ(this->getValue(1), "100");
        } else {
            if (hasPositionSensor && time == 10000) {
                EXPECT_EQ(this->getValue(1), "0");
            } else {
                EXPECT_EQ(this->getValue(1), "99");
            }
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(10000, delay, func2));
    ASSERT_NO_FAILURE();

    ASSERT_NO_FATAL_FAILURE(
        this->loopFor(delay, delay, [&](unsigned long, size_t) {
        EXPECT_TRUE(this->isMovingUp());
        EXPECT_EQ(this->getValue(0), "CLOSED");
        EXPECT_EQ(this->getValue(1), "0");
    }));
    ASSERT_NO_FAILURE();

    if (hasPositionSensor && start == 0) {
        std::cout << "After a known full open-close cycle, calibration is "
                     "done. Skip phase 3, set position."
                  << std::endl;
        auto func4 = [&](unsigned long time, size_t /*round*/) {
            EXPECT_TRUE(this->isMovingUp());
            EXPECT_EQ(this->getValue(0), "OPENING");
            EXPECT_EQ(
                this->getValue(1),
                std::to_string((time - delay) * 100 / this->maxPosition));
        };
        ASSERT_NO_FATAL_FAILURE(this->loopFor(4000, delay, func4));
        ASSERT_NO_FAILURE();

        ASSERT_NO_FATAL_FAILURE(
            this->loopFor(delay, delay, [&](unsigned long, size_t) {
            EXPECT_FALSE(this->isMovingUp());
            EXPECT_FALSE(this->isMovingDown());
            EXPECT_EQ(this->getValue(0), "OPENING");
            EXPECT_EQ(this->getValue(1), "40");
        }));
        ASSERT_NO_FAILURE();

        EXPECT_EQ(this->position, 4000 + delay);
    } else {
        std::cout
            << "Phase 3: move from fully closed to fully open, calculating "
               "opening time."
            << std::endl;
        auto func3 = [&](unsigned long time, size_t round) {
            EXPECT_TRUE(this->isMovingUp());
            EXPECT_EQ(this->getValue(0), "OPENING");
            if (!hasPositionSensor && (this->isDebouncing(time, round))) {
                EXPECT_EQ(this->getValue(1), "0");
            } else {
                if (hasPositionSensor && time == 10000) {
                    EXPECT_EQ(this->getValue(1), "100");
                } else {
                    EXPECT_EQ(this->getValue(1), "1");
                }
            }
        };
        ASSERT_NO_FATAL_FAILURE(this->loopFor(10000, delay, func3));
        ASSERT_NO_FAILURE();

        std::cout << "Calibration is done, set position." << std::endl;

        ASSERT_NO_FATAL_FAILURE(
            this->loopFor(delay, delay, [&](unsigned long, size_t) {
            EXPECT_TRUE(this->isMovingDown());
            EXPECT_EQ(this->getValue(0), "OPEN");
            EXPECT_EQ(this->getValue(1), "100");
        }));
        ASSERT_NO_FAILURE();

        auto func4 = [&](unsigned long time, size_t round) {
            EXPECT_TRUE(this->isMovingDown());
            EXPECT_EQ(this->getValue(0), "CLOSING");
            if (this->isDebouncing(time, round)) {
                EXPECT_EQ(this->getValue(1), "100");
            } else {
                EXPECT_EQ(
                    this->getValue(1),
                    std::to_string(
                        100 - (time - delay) * 100 / this->maxPosition));
            }
        };
        ASSERT_NO_FATAL_FAILURE(this->loopFor(6000, delay, func4));
        ASSERT_NO_FAILURE();

        auto func5 = [&](unsigned long /*time*/, size_t round) {
            EXPECT_FALSE(this->isMovingDown());
            EXPECT_EQ(this->getValue(1), "40");
            if (round == 1) {
                EXPECT_EQ(this->getValue(0), "CLOSING");
            } else {
                EXPECT_EQ(this->getValue(0), "OPEN");
            }
        };
        ASSERT_NO_FATAL_FAILURE(this->loopFor(delay * 3, delay, func5));
        ASSERT_NO_FAILURE();

        EXPECT_EQ(this->position, 4000 - delay);
    }

    EXPECT_FALSE(this->isMovingUp());
    EXPECT_FALSE(this->isMovingDown());
}

INSTANTIATE_TEST_SUITE_P(
    Calibrate, CalibrateFixture,
    testing::Combine(
        testing::ValuesIn(delays1), testing::ValuesIn(latchings),
        testing::ValuesIn(hasPositionSensorValues),
        testing::ValuesIn(calibrateStartPositions)));

TEST_P(OpenAfterCalibrateFixture, OpenAfterCalibrate) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    ASSERT_NO_FATAL_FAILURE(this->calibrateToPosition(60, delay));
    this->open();
    auto func = [&](unsigned long time, size_t round) {
        if (this->isDebouncing(time, round)) {
            EXPECT_TRUE(this->isMovingUp());
            EXPECT_EQ(this->getValue(0), "OPENING");
            EXPECT_EQ(this->getValue(1), "60");
        } else if (time <= 4000) {
            EXPECT_TRUE(this->isMovingUp());
            EXPECT_EQ(this->getValue(0), "OPENING");
            EXPECT_EQ(
                this->getValue(1),
                std::to_string(60 + (time - delay) * 100 / this->maxPosition));
        } else if (time <= static_cast<unsigned long>(4000 + delay)) {
            EXPECT_TRUE(this->isMovingUp());
            EXPECT_EQ(this->getValue(0), "OPENING");
            if (hasPositionSensor) {
                EXPECT_EQ(this->getValue(1), "100");
            } else {
                EXPECT_EQ(this->getValue(1), "99");
            }
        } else {
            EXPECT_FALSE(this->isMovingUp());
            EXPECT_EQ(this->getValue(0), "OPEN");
            EXPECT_EQ(this->getValue(1), "100");
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(4200, delay, func));
}

INSTANTIATE_TEST_SUITE_P(
    OpenAfterCalibrate, OpenAfterCalibrateFixture,
    testing::Combine(
        testing::ValuesIn(delays2), testing::ValuesIn(latchings),
        testing::ValuesIn(hasPositionSensorValues)));

TEST_P(CloseAfterCalibrateFixture, CloseAfterCalibrate) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    ASSERT_NO_FATAL_FAILURE(this->calibrateToPosition(60, delay));
    this->close();
    auto func = [&](unsigned long time, size_t round) {
        if (this->isDebouncing(time, round)) {
            EXPECT_TRUE(this->isMovingDown());
            EXPECT_EQ(this->getValue(0), "CLOSING");
            EXPECT_EQ(this->getValue(1), "60");
        } else if (time <= static_cast<unsigned long>(6000 - delay)) {
            EXPECT_TRUE(this->isMovingDown());
            EXPECT_EQ(this->getValue(0), "CLOSING");
            if (hasPositionSensor &&
                time == static_cast<unsigned long>(6000 - delay)) {
                EXPECT_EQ(this->getValue(1), "0");
            } else {
                EXPECT_EQ(
                    this->getValue(1),
                    std::to_string(
                        60 - (time - delay) * 100 / this->maxPosition));
            }
        } else {
            EXPECT_FALSE(this->isMovingDown());
            EXPECT_EQ(this->getValue(0), "CLOSED");
            EXPECT_EQ(this->getValue(1), "0");
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(6200, delay, func));
}

INSTANTIATE_TEST_SUITE_P(
    CloseAfterCalibrate, CloseAfterCalibrateFixture,
    testing::Combine(
        testing::ValuesIn(delays2), testing::ValuesIn(latchings),
        testing::ValuesIn(hasPositionSensorValues)));

TEST_P(RestartAfterCalibrateFixture, RestartAfterCalibrate) {
    GET_PARAM(delay, 0);
    GET_PARAM(isLatching, 1);
    GET_PARAM(hasPositionSensor, 2);

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    ASSERT_NO_FATAL_FAILURE(this->calibrateToPosition(60, delay));
    this->reboot();
    this->position = 6000;
    this->loop();
    this->setPosition(40);

    auto func4 = [&](unsigned long time, size_t round) {
        EXPECT_TRUE(this->isMovingDown());
        EXPECT_EQ(this->getValue(0), "CLOSING");
        if (this->isDebouncing(time, round)) {
            EXPECT_EQ(this->getValue(1), "60");
        } else {
            EXPECT_EQ(
                this->getValue(1),
                std::to_string(60 - (time - delay) * 100 / this->maxPosition));
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(2000, delay, func4));
    ASSERT_NO_FAILURE();

    auto func5 = [&](unsigned long /*time*/, size_t round) {
        EXPECT_FALSE(this->isMovingDown());
        EXPECT_EQ(this->getValue(1), "40");
        if (round == 1) {
            EXPECT_EQ(this->getValue(0), "CLOSING");
        } else {
            EXPECT_EQ(this->getValue(0), "OPEN");
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(delay * 3, delay, func5));
    ASSERT_NO_FAILURE();
    EXPECT_EQ(this->position, 4000 - delay);
    EXPECT_FALSE(this->isMovingUp());
    EXPECT_FALSE(this->isMovingDown());
}

INSTANTIATE_TEST_SUITE_P(
    RestartAfterCalibrate, RestartAfterCalibrateFixture,
    testing::Combine(
        testing::ValuesIn(delays2), testing::ValuesIn(latchings),
        testing::ValuesIn(hasPositionSensorValues)));

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

    this->open();

    std::cerr << "Phase 1: opening time is not known, only position sensors "
                 "are reported."
              << std::endl;

    auto func1 = [&](unsigned long time, size_t /*round*/) {
        EXPECT_TRUE(this->isMovingUp());
        EXPECT_EQ(this->getValue(0), "OPENING");
        if (time <= 200) {
            EXPECT_EQ(this->getValue(1), "0");
        } else if (time < 4800) {
            EXPECT_EQ(this->getValue(1), "1");
        } else if (time <= 5200) {
            EXPECT_EQ(this->getValue(1), "50");
        } else if (time < 9800) {
            EXPECT_EQ(this->getValue(1), "51");
        } else {
            EXPECT_EQ(this->getValue(1), "100");
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(10000, delay, func1));
    ASSERT_NO_FAILURE();

    auto funcOpen = [&](unsigned long time, size_t /*round*/) {
        if (time < static_cast<unsigned long>(delay)) {
        } else {
            EXPECT_FALSE(this->isMovingUp());
            EXPECT_FALSE(this->isMovingDown());
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(delay, delay, funcOpen));
    ASSERT_NO_FAILURE();

    this->close();

    std::cerr << "Phase 2: closing time is not known, only position sensors "
                 "are reported."
              << std::endl;

    auto func2 = [&](unsigned long time, size_t /*round*/) {
        EXPECT_TRUE(this->isMovingDown());
        EXPECT_EQ(this->getValue(0), "CLOSING");
        if (time <= 200) {
            EXPECT_EQ(this->getValue(1), "100");
        } else if (time < 4800) {
            EXPECT_EQ(this->getValue(1), "99");
        } else if (time <= 5200) {
            EXPECT_EQ(this->getValue(1), "50");
        } else if (time < 9800) {
            EXPECT_EQ(this->getValue(1), "49");
        } else {
            EXPECT_EQ(this->getValue(1), "0");
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(10000, delay, func2));
    ASSERT_NO_FAILURE();

    auto funcClosed = [&](unsigned long /*time*/, size_t /*round*/) {
        EXPECT_FALSE(this->isMovingUp());
        EXPECT_FALSE(this->isMovingDown());
        EXPECT_EQ(this->getValue(0), "CLOSED");
        EXPECT_EQ(this->getValue(1), "0");
        EXPECT_EQ(this->position, 0);
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(delay, delay, funcClosed));
    ASSERT_NO_FAILURE();

    this->open();

    std::cerr << "Phase 3: opening time is known, positions are interpolated "
                 "between position sensos."
              << std::endl;

    auto func3 = [&](unsigned long time, size_t /*round*/) {
        EXPECT_TRUE(this->isMovingUp());
        EXPECT_EQ(this->getValue(0), "OPENING");
        if (time <= 200) {
            EXPECT_EQ(this->getValue(1), "0");
        } else if (time < 4800) {
            EXPECT_EQ(
                this->getValue(1),
                std::to_string((time - 200 - delay) * 50 / (4600 - delay)));
        } else if (time <= 5200) {
            EXPECT_EQ(this->getValue(1), "50");
        } else if (time < 9800) {
            EXPECT_EQ(
                this->getValue(1),
                std::to_string(
                    50 + (time - 5200 - delay) * 50 / (4600 - delay)));
        } else {
            EXPECT_EQ(this->getValue(1), "100");
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(10000, delay, func3));
    ASSERT_NO_FAILURE();
    ASSERT_NO_FATAL_FAILURE(this->loopFor(delay, delay, funcOpen));
    ASSERT_NO_FAILURE();

    this->close();

    std::cerr << "Phase 4: closing time is known, positions are interpolated "
                 "between position sensos."
              << std::endl;

    auto func4 = [&](unsigned long time, size_t /*round*/) {
        EXPECT_TRUE(this->isMovingDown());
        EXPECT_EQ(this->getValue(0), "CLOSING");
        if (time <= 200) {
            EXPECT_EQ(this->getValue(1), "100");
        } else if (time < 4800) {
            EXPECT_EQ(
                this->getValue(1),
                std::to_string(
                    100 - (time - 200 - delay) * 50 / (4600 - delay)));
        } else if (time <= 5200) {
            EXPECT_EQ(this->getValue(1), "50");
        } else if (time < 9800) {
            EXPECT_EQ(
                this->getValue(1),
                std::to_string(
                    50 - (time - 5200 - delay) * 50 / (4600 - delay)));
        } else {
            EXPECT_EQ(this->getValue(1), "0");
        }
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(10000, delay, func4));
    ASSERT_NO_FAILURE();
    ASSERT_NO_FATAL_FAILURE(this->loopFor(delay, delay, funcClosed));
}

INSTANTIATE_TEST_SUITE_P(
    MultiplePositionSensors, MultiplePositionSensorsFixture,
    testing::Combine(
        testing::ValuesIn(delays2), testing::ValuesIn(latchings),
        testing::Values(false, true), testing::Values(false, true),
        testing::Values(false, true)));

class StopEarlyWhileCalibratingFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<int, bool, bool>> {};

TEST_P(StopEarlyWhileCalibratingFixture, StopEarlyWhileCalibrating) {
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

    auto checkNotMoving = [&](unsigned long, size_t) {
        EXPECT_FALSE(this->isMovingUp());
        EXPECT_FALSE(this->isMovingDown());
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(delay * 3, delay, checkNotMoving));
    ASSERT_NO_FAILURE();
}

class StopMomentarilyWhileCalibratingFixture
    : public CoverTest,
      public ::testing::WithParamInterface<std::tuple<int, bool>> {};

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

    this->movingUp = false;
    this->loop();

    auto checkNotMoving = [&](unsigned long, size_t) {
        EXPECT_FALSE(this->isMovingUp());
        EXPECT_FALSE(this->isMovingDown());
    };
    ASSERT_NO_FATAL_FAILURE(this->loopFor(delay * 3, delay, checkNotMoving));
}

INSTANTIATE_TEST_SUITE_P(
    StopMomentarilyWhileCalibrating, StopMomentarilyWhileCalibratingFixture,
    testing::Combine(testing::ValuesIn(delays2), testing::ValuesIn(latchings)));

INSTANTIATE_TEST_SUITE_P(
    StopEarlyWhileCalibrating, StopEarlyWhileCalibratingFixture,
    testing::Combine(
        testing::ValuesIn(delays1), testing::ValuesIn(latchings),
        testing::ValuesIn(hasPositionSensorValues)));
