#include <algorithm>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <tuple>

#include "InterfaceTestBase.hpp"
#include "common/Cover.hpp"

BOOST_AUTO_TEST_SUITE(CoverTest)

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

class Fixture : public InterfaceTestBase {
public:
    const int maxPosition = 10000;
    bool latching = false;
    int position = 0;
    bool isWorking = true;
    unsigned long previousTime = 0;
    bool movingUp = false;
    bool movingDown = false;
    std::vector<TestPositionSensor> positionSensors;

    Fixture() {}

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
            "cover",
            std::make_unique<Cover>(
                this->debug, this->esp, this->rtc, UpInput, DownInput, UpOutput,
                DownOutput, isLatching ? StopOutput : 0, false, false, 10,
                std::move(positionSensorInput), false));
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
        BOOST_TEST_MESSAGE("Loop begin, time=" << now);
        int delta = now - this->previousTime;
        const bool upOn = this->esp.digitalRead(UpOutput) != 0;
        const bool downOn = this->esp.digitalRead(DownOutput) != 0;

        if (this->isWorking) {
            if (upOn && downOn) {
                BOOST_FAIL("Should not try to move in both directions.");
            }

            if (this->latching) {
                if (upOn) {
                    BOOST_TEST_MESSAGE("Start moving up");
                    this->movingUp = true;
                    this->movingDown = false;
                } else if (downOn) {
                    BOOST_TEST_MESSAGE("Start moving down");
                    this->movingUp = false;
                    this->movingDown = true;
                } else if (this->esp.digitalRead(StopOutput) != 0) {
                    BOOST_TEST_MESSAGE("Stop");
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

        BOOST_TEST_MESSAGE(
            "upOn=" << upOn << " downOn=" << downOn << " movingUp="
                    << this->movingUp << " movingDown=" << this->movingDown
                    << " position=" << newPosition << " movedUp=" << movedUp
                    << " movedDown=" << movedDown);

        this->esp.digitalWrite(UpInput, movedUp);
        this->esp.digitalWrite(DownInput, movedDown);

        this->position = newPosition;
        this->previousTime = now;

        this->updateInterface();

        BOOST_TEST_MESSAGE("Loop end");
    }

    void open() {
        BOOST_TEST_MESSAGE("Open");
        this->interface.interface->execute("OPEN");
    }

    void close() {
        BOOST_TEST_MESSAGE("Close");
        this->interface.interface->execute("CLOSE");
    }

    void stop() {
        BOOST_TEST_MESSAGE("Stop");
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
        BOOST_TEST_MESSAGE("---- loopFor " << time << "----");
        this->delayUntil(beginTime + time, delay, [&]() {
            this->loop();
            auto time = this->esp.millis() - beginTime;
            ++round;
            BOOST_TEST_CONTEXT(
                "round=" << round << " time=" << time
                         << " position=" << this->position) {
                BOOST_REQUIRE_NO_THROW(func(time, round));
            }
        });
        BOOST_TEST_MESSAGE("---- loopFor done ----");
    }

    void calibrateToPosition(int position, unsigned long delay) {
        this->setPosition(position);
        this->loopFor(41000, delay, [](unsigned long, size_t) {});
        if (position <= 10) {
            BOOST_TEST_REQUIRE(this->getValue(0) == "CLOSED");
        } else {
            BOOST_TEST_REQUIRE(this->getValue(0) == "OPEN");
        }
        BOOST_TEST_REQUIRE(this->getValue(1) == std::to_string(position));
        BOOST_TEST_MESSAGE("---- Calibration done ----");
    }
};

namespace {
const auto hasPositionSensorValues =
    boost::unit_test::data::make({false, true});
}

BOOST_DATA_TEST_CASE_F(
    Fixture, NormalMode, hasPositionSensorValues, hasPositionSensor) {
    auto check = [this](const std::string& name, int upValue, int downValue) {
        BOOST_TEST_CONTEXT(name) {
            BOOST_TEST(this->esp.digitalRead(UpOutput) == upValue);
            BOOST_TEST(this->esp.digitalRead(DownOutput) == downValue);
            this->esp.delay(10);
            this->loop();
            BOOST_TEST(this->esp.digitalRead(UpOutput) == upValue);
            BOOST_TEST(this->esp.digitalRead(DownOutput) == downValue);
        }
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

BOOST_DATA_TEST_CASE_F(
    Fixture, LatchingMode, hasPositionSensorValues, hasPositionSensor) {
    auto check = [this](
                     const std::string& name, int upValue, int downValue,
                     int stopValue) {
        BOOST_TEST_CONTEXT(name) {
            BOOST_TEST(this->esp.digitalRead(UpOutput) == upValue);
            BOOST_TEST(this->esp.digitalRead(DownOutput) == downValue);
            BOOST_TEST(this->esp.digitalRead(StopOutput) == stopValue);
            this->esp.delay(10);
            this->loop();
            BOOST_TEST(this->esp.digitalRead(UpOutput) == 0);
            BOOST_TEST(this->esp.digitalRead(DownOutput) == 0);
            BOOST_TEST(this->esp.digitalRead(StopOutput) == 0);
        }
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

namespace {
const auto delays1 = boost::unit_test::data::make({10, 50, 100, 500});
const auto delays2 = boost::unit_test::data::make({10, 50, 100});
const auto latchings = boost::unit_test::data::make({false, true});
const auto params1 = delays1 * latchings * hasPositionSensorValues;
const auto params2 = delays2 * latchings * hasPositionSensorValues;
}  // namespace
//

BOOST_DATA_TEST_CASE_F(
    Fixture, Open, params1, delay, isLatching, hasPositionSensor) {
    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->loop();

    this->open();
    auto func = [&](unsigned long time, size_t round) {
        if (!hasPositionSensor && (time <= 20 || round == 1)) {
            BOOST_TEST(this->isMovingUp());
            BOOST_TEST(this->interface.storedValue.size() == 1);
            BOOST_TEST(this->getValue(0) == "OPENING");
        } else if (time <= 10000) {
            BOOST_TEST(this->isMovingUp());
            BOOST_TEST(this->getValue(0) == "OPENING");
            if (hasPositionSensor && time == 10000) {
                BOOST_TEST(this->getValue(1) == "100");
            } else {
                BOOST_TEST(this->getValue(1) == "1");
            }
        } else {
            BOOST_TEST(!this->isMovingUp());
            BOOST_TEST(this->getValue(0) == "OPEN");
            BOOST_TEST(this->getValue(1) == "100");
        }
    };
    BOOST_REQUIRE_NO_THROW(this->loopFor(10100, delay, func));
}

BOOST_DATA_TEST_CASE_F(
    Fixture, Close, params1, delay, isLatching, hasPositionSensor) {
    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->position = 10000;
    this->loop();

    this->close();
    auto func = [&](unsigned long time, size_t round) {
        if (!hasPositionSensor && (time <= 20 || round == 1)) {
            BOOST_TEST(this->isMovingDown());
            BOOST_TEST(this->interface.storedValue.size() == 1);
            BOOST_TEST(this->getValue(0) == "CLOSING");
        } else if (time <= 10000) {
            BOOST_TEST(this->isMovingDown());
            BOOST_TEST(this->getValue(0) == "CLOSING");
            if (hasPositionSensor && time == 10000) {
                BOOST_TEST(this->getValue(1) == "0");
            } else {
                BOOST_TEST(this->getValue(1) == "99");
            }
        } else {
            BOOST_TEST(!this->isMovingDown());
            BOOST_TEST(this->getValue(0) == "CLOSED");
            BOOST_TEST(this->getValue(1) == "0");
        }
    };
    BOOST_REQUIRE_NO_THROW(this->loopFor(10100, delay, func));
}

BOOST_DATA_TEST_CASE_F(
    Fixture, StopWhileOpening, params1, delay, isLatching, hasPositionSensor) {
    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->loop();

    this->open();
    BOOST_REQUIRE_NO_THROW(
        this->loopFor(2000, delay, [](unsigned long, size_t) {}));

    this->stop();
    this->esp.delay(delay);
    this->loop();

    BOOST_TEST(!this->isMovingUp());
    BOOST_TEST(!this->isMovingDown());
    BOOST_TEST(this->position == 2000);
}

BOOST_DATA_TEST_CASE_F(
    Fixture, StopWhileClosing, params1, delay, isLatching, hasPositionSensor) {
    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->position = 10000;
    this->loop();

    this->close();
    BOOST_REQUIRE_NO_THROW(
        this->loopFor(2000, delay, [](unsigned long, size_t) {}));

    this->stop();
    this->esp.delay(delay);
    this->loop();

    BOOST_TEST(!this->isMovingUp());
    BOOST_TEST(!this->isMovingDown());
    BOOST_TEST(this->position == 8000);
}

namespace {
const auto calibrateStartPositions =
    boost::unit_test::data::make({0, 5000, 8000, 10000});
const auto calibrateParams = params1 * calibrateStartPositions;
}  // namespace

BOOST_DATA_TEST_CASE_F(
    Fixture, Calibrate, calibrateParams, delay, isLatching, hasPositionSensor,
    start) {
    if (hasPositionSensor && delay == 500) {
        return;
    }

    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    this->position = start;
    this->loop();

    this->setPosition(40);

    if (start == this->maxPosition) {
        if (!hasPositionSensor) {
            auto func1 = [&](unsigned long, size_t) {
                BOOST_TEST(this->isMovingUp());
                BOOST_TEST(this->position == this->maxPosition);
                BOOST_TEST(this->interface.storedValue.size() == 1);
                BOOST_TEST(this->getValue(0) == "OPENING");
            };
            BOOST_REQUIRE_NO_THROW(this->loopFor(1000, delay, func1));
        } else {
            BOOST_TEST(this->getValue(0) == "OPEN");
            BOOST_TEST(this->getValue(1) == "100");
        }
    } else {
        const unsigned long travelTime = 10000 - start;
        auto func1 = [&](unsigned long time, size_t round) {
            BOOST_TEST(this->isMovingUp());
            BOOST_TEST(this->getValue(0) == "OPENING");
            if (!(hasPositionSensor && start == 0) &&
                (time <= 20 || round == 1)) {
                BOOST_TEST(this->interface.storedValue.size() == 1);
            } else {
                if (hasPositionSensor && time == travelTime) {
                    BOOST_TEST(this->getValue(1) == "100");
                } else {
                    BOOST_TEST(this->getValue(1) == "1");
                }
            }
        };
        BOOST_REQUIRE_NO_THROW(this->loopFor(travelTime, delay, func1));
    }

    if (!(hasPositionSensor && start == this->maxPosition)) {
        auto funcOpen = [&](unsigned long, size_t) {
            BOOST_TEST(this->isMovingDown());
            BOOST_TEST(this->getValue(0) == "OPEN");
            BOOST_TEST(this->getValue(1) == "100");
        };

        BOOST_REQUIRE_NO_THROW(this->loopFor(delay, delay, funcOpen));
    }

    auto func2 = [&](unsigned long time, size_t round) {
        BOOST_TEST(this->isMovingDown());
        BOOST_TEST(this->getValue(0) == "CLOSING");
        if (!hasPositionSensor && (time <= 20 || round == 1)) {
            BOOST_TEST(this->getValue(1) == "100");
        } else {
            if (hasPositionSensor && time == 10000) {
                BOOST_TEST(this->getValue(1) == "0");
            } else {
                BOOST_TEST(this->getValue(1) == "99");
            }
        }
    };
    BOOST_REQUIRE_NO_THROW(this->loopFor(10000, delay, func2));

    BOOST_REQUIRE_NO_THROW(
        this->loopFor(delay, delay, [&](unsigned long, size_t) {
        BOOST_TEST(this->isMovingUp());
        BOOST_TEST(this->getValue(0) == "CLOSED");
        BOOST_TEST(this->getValue(1) == "0");
    }));

    if (hasPositionSensor && start == 0) {
        auto func4 = [&](unsigned long time, size_t /*round*/) {
            BOOST_TEST(this->isMovingUp());
            BOOST_TEST(this->getValue(0) == "OPENING");
            BOOST_TEST(
                this->getValue(1) ==
                std::to_string((time - delay) * 100 / this->maxPosition));
        };
        BOOST_REQUIRE_NO_THROW(this->loopFor(4000, delay, func4));

        BOOST_REQUIRE_NO_THROW(
            this->loopFor(delay, delay, [&](unsigned long, size_t) {
            BOOST_TEST(!this->isMovingUp());
            BOOST_TEST(!this->isMovingDown());
            BOOST_TEST(this->getValue(0) == "OPENING");
            BOOST_TEST(this->getValue(1) == "40");
        }));

        BOOST_TEST(this->position == 4000 + delay);
    } else {
        auto func3 = [&](unsigned long time, size_t round) {
            BOOST_TEST(this->isMovingUp());
            BOOST_TEST(this->getValue(0) == "OPENING");
            if (!hasPositionSensor && (time <= 20 || round == 1)) {
                BOOST_TEST(this->getValue(1) == "0");
            } else {
                if (hasPositionSensor && time == 10000) {
                    BOOST_TEST(this->getValue(1) == "100");
                } else {
                    BOOST_TEST(this->getValue(1) == "1");
                }
            }
        };
        BOOST_REQUIRE_NO_THROW(this->loopFor(10000, delay, func3));

        BOOST_REQUIRE_NO_THROW(
            this->loopFor(delay, delay, [&](unsigned long, size_t) {
            BOOST_TEST(this->isMovingDown());
            BOOST_TEST(this->getValue(0) == "OPEN");
            BOOST_TEST(this->getValue(1) == "100");
        }));

        auto func4 = [&](unsigned long time, size_t round) {
            BOOST_TEST(this->isMovingDown());
            BOOST_TEST(this->getValue(0) == "CLOSING");
            if (time <= 20 || round == 1) {
                BOOST_TEST(this->getValue(1) == "100");
            } else {
                BOOST_TEST(
                    this->getValue(1) ==
                    std::to_string(
                        100 - (time - delay) * 100 / this->maxPosition));
            }
        };
        BOOST_REQUIRE_NO_THROW(this->loopFor(6000, delay, func4));

        auto func5 = [&](unsigned long /*time*/, size_t round) {
            BOOST_TEST(!this->isMovingDown());
            BOOST_TEST(this->getValue(1) == "40");
            if (round == 1) {
                BOOST_TEST(this->getValue(0) == "CLOSING");
            } else {
                BOOST_TEST(this->getValue(0) == "OPEN");
            }
        };
        BOOST_REQUIRE_NO_THROW(this->loopFor(delay * 3, delay, func5));

        BOOST_TEST(this->position == 4000 - delay);
    }

    BOOST_TEST(!this->isMovingUp());
    BOOST_TEST(!this->isMovingDown());
}

BOOST_DATA_TEST_CASE_F(
    Fixture, OpenAfterCalibrate, params2, delay, isLatching,
    hasPositionSensor) {
    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    BOOST_REQUIRE_NO_THROW(this->calibrateToPosition(60, delay));
    this->open();
    auto func = [&](unsigned long time, size_t round) {
        if (time <= 20 || round == 1) {
            BOOST_TEST(this->isMovingUp());
            BOOST_TEST(this->getValue(0) == "OPENING");
            BOOST_TEST(this->getValue(1) == "60");
        } else if (time <= 4000) {
            BOOST_TEST(this->isMovingUp());
            BOOST_TEST(this->getValue(0) == "OPENING");
            BOOST_TEST(
                this->getValue(1) ==
                std::to_string(60 + (time - delay) * 100 / this->maxPosition));
        } else if (time <= static_cast<unsigned long>(4000 + delay)) {
            BOOST_TEST(this->isMovingUp());
            BOOST_TEST(this->getValue(0) == "OPENING");
            if (hasPositionSensor) {
                BOOST_TEST(this->getValue(1) == "100");
            } else {
                BOOST_TEST(this->getValue(1) == "99");
            }
        } else {
            BOOST_TEST(!this->isMovingUp());
            BOOST_TEST(this->getValue(0) == "OPEN");
            BOOST_TEST(this->getValue(1) == "100");
        }
    };
    BOOST_REQUIRE_NO_THROW(this->loopFor(4200, delay, func));
}

BOOST_DATA_TEST_CASE_F(
    Fixture, CloseAfterCalibrate, params2, delay, isLatching,
    hasPositionSensor) {
    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    BOOST_REQUIRE_NO_THROW(this->calibrateToPosition(60, delay));
    this->close();
    auto func = [&](unsigned long time, size_t round) {
        if (time <= 20 || round == 1) {
            BOOST_TEST(this->isMovingDown());
            BOOST_TEST(this->getValue(0) == "CLOSING");
            BOOST_TEST(this->getValue(1) == "60");
        } else if (time <= static_cast<unsigned long>(6000 - delay)) {
            BOOST_TEST(this->isMovingDown());
            BOOST_TEST(this->getValue(0) == "CLOSING");
            if (hasPositionSensor &&
                time == static_cast<unsigned long>(6000 - delay)) {
                BOOST_TEST(this->getValue(1) == "0");
            } else {
                BOOST_TEST(
                    this->getValue(1) ==
                    std::to_string(
                        60 - (time - delay) * 100 / this->maxPosition));
            }
        } else {
            BOOST_TEST(!this->isMovingDown());
            BOOST_TEST(this->getValue(0) == "CLOSED");
            BOOST_TEST(this->getValue(1) == "0");
        }
    };
    BOOST_REQUIRE_NO_THROW(this->loopFor(6200, delay, func));
}

BOOST_DATA_TEST_CASE_F(
    Fixture, RestartAfterCalibrate, params2, delay, isLatching,
    hasPositionSensor) {
    this->init(isLatching, this->getPositionSensors(hasPositionSensor));
    BOOST_REQUIRE_NO_THROW(this->calibrateToPosition(60, delay));
    this->reboot();
    this->position = 6000;
    this->loop();
    this->setPosition(40);

    auto func4 = [&](unsigned long time, size_t round) {
        BOOST_TEST(this->isMovingDown());
        BOOST_TEST(this->getValue(0) == "CLOSING");
        if (time <= 20 || round == 1) {
            BOOST_TEST(this->getValue(1) == "60");
        } else {
            BOOST_TEST(
                this->getValue(1) ==
                std::to_string(60 - (time - delay) * 100 / this->maxPosition));
        }
    };
    BOOST_REQUIRE_NO_THROW(this->loopFor(2000, delay, func4));

    auto func5 = [&](unsigned long /*time*/, size_t round) {
        BOOST_TEST(!this->isMovingDown());
        BOOST_TEST(this->getValue(1) == "40");
        if (round == 1) {
            BOOST_TEST(this->getValue(0) == "CLOSING");
        } else {
            BOOST_TEST(this->getValue(0) == "OPEN");
        }
    };
    BOOST_REQUIRE_NO_THROW(this->loopFor(delay * 3, delay, func5));
    BOOST_TEST(this->position == 4000 - delay);
    BOOST_TEST(!this->isMovingUp());
    BOOST_TEST(!this->isMovingDown());
}

namespace {
const auto params2NoPositionSensor =
    delays2 * latchings * boost::unit_test::data::make({false, true}) *
    boost::unit_test::data::make({false, true}) *
    boost::unit_test::data::make({false, true});

}  // namespace

BOOST_DATA_TEST_CASE_F(
    Fixture, MultiplePositionSensors, params2NoPositionSensor, delay,
    isLatching, invertClosed, invertMiddle, invertOpen) {
    this->init(
        isLatching, {
                        TestPositionSensor{0, 0, 200, invertClosed},
                        TestPositionSensor{50, 4800, 5200, invertMiddle},
                        TestPositionSensor{100, 9800, 10000, invertOpen},
                    });
    this->loop();

    BOOST_TEST(!this->isMovingUp());
    BOOST_TEST(!this->isMovingDown());
    BOOST_TEST(this->getValue(0) == "CLOSED");
    BOOST_TEST(this->getValue(1) == "0");

    this->open();

    auto func1 = [&](unsigned long time, size_t /*round*/) {
        BOOST_TEST(this->isMovingUp());
        BOOST_TEST(this->getValue(0) == "OPENING");
        if (time <= 200) {
            BOOST_TEST(this->getValue(1) == "0");
        } else if (time < 4800) {
            BOOST_TEST(this->getValue(1) == "1");
        } else if (time <= 5200) {
            BOOST_TEST(this->getValue(1) == "50");
        } else if (time < 9800) {
            BOOST_TEST(this->getValue(1) == "51");
        } else {
            BOOST_TEST(this->getValue(1) == "100");
        }
    };
    BOOST_REQUIRE_NO_THROW(this->loopFor(10000, delay, func1));

    auto funcOpen = [&](unsigned long /*time*/, size_t /*round*/) {
        BOOST_TEST(!this->isMovingUp());
        BOOST_TEST(!this->isMovingDown());
        BOOST_TEST(this->getValue(0) == "OPEN");
        BOOST_TEST(this->getValue(1) == "100");
        BOOST_TEST(this->position == 10000);
    };
    BOOST_REQUIRE_NO_THROW(this->loopFor(delay, delay, funcOpen));

    this->close();

    auto func2 = [&](unsigned long time, size_t /*round*/) {
        BOOST_TEST(this->isMovingDown());
        BOOST_TEST(this->getValue(0) == "CLOSING");
        if (time <= 200) {
            BOOST_TEST(this->getValue(1) == "100");
        } else if (time < 4800) {
            BOOST_TEST(this->getValue(1) == "99");
        } else if (time <= 5200) {
            BOOST_TEST(this->getValue(1) == "50");
        } else if (time < 9800) {
            BOOST_TEST(this->getValue(1) == "49");
        } else {
            BOOST_TEST(this->getValue(1) == "0");
        }
    };
    BOOST_REQUIRE_NO_THROW(this->loopFor(10000, delay, func2));

    auto funcClosed = [&](unsigned long /*time*/, size_t /*round*/) {
        BOOST_TEST(!this->isMovingUp());
        BOOST_TEST(!this->isMovingDown());
        BOOST_TEST(this->getValue(0) == "CLOSED");
        BOOST_TEST(this->getValue(1) == "0");
        BOOST_TEST(this->position == 0);
    };
    BOOST_REQUIRE_NO_THROW(this->loopFor(delay, delay, funcClosed));

    this->open();

    auto func3 = [&](unsigned long time, size_t /*round*/) {
        BOOST_TEST(this->isMovingUp());
        BOOST_TEST(this->getValue(0) == "OPENING");
        if (time <= 200) {
            BOOST_TEST(this->getValue(1) == "0");
        } else if (time < 4800) {
            BOOST_TEST(
                this->getValue(1) ==
                std::to_string((time - 200 - delay) * 50 / (4600 - delay)));
        } else if (time <= 5200) {
            BOOST_TEST(this->getValue(1) == "50");
        } else if (time < 9800) {
            BOOST_TEST(
                this->getValue(1) ==
                std::to_string(
                    50 + (time - 5200 - delay) * 50 / (4600 - delay)));
        } else {
            BOOST_TEST(this->getValue(1) == "100");
        }
    };
    BOOST_REQUIRE_NO_THROW(this->loopFor(10000, delay, func3));
    BOOST_REQUIRE_NO_THROW(this->loopFor(delay, delay, funcOpen));

    this->close();

    auto func4 = [&](unsigned long time, size_t /*round*/) {
        BOOST_TEST(this->isMovingDown());
        BOOST_TEST(this->getValue(0) == "CLOSING");
        if (time <= 200) {
            BOOST_TEST(this->getValue(1) == "100");
        } else if (time < 4800) {
            BOOST_TEST(
                this->getValue(1) ==
                std::to_string(
                    100 - (time - 200 - delay) * 50 / (4600 - delay)));
        } else if (time <= 5200) {
            BOOST_TEST(this->getValue(1) == "50");
        } else if (time < 9800) {
            BOOST_TEST(
                this->getValue(1) ==
                std::to_string(
                    50 - (time - 5200 - delay) * 50 / (4600 - delay)));
        } else {
            BOOST_TEST(this->getValue(1) == "0");
        }
    };
    BOOST_REQUIRE_NO_THROW(this->loopFor(10000, delay, func4));
    BOOST_REQUIRE_NO_THROW(this->loopFor(delay, delay, funcClosed));
}

BOOST_AUTO_TEST_SUITE_END();
