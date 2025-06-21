#include <algorithm>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <boost/test/unit_test_suite.hpp>

#include "InterfaceTestBase.hpp"
#include "common/Cover.hpp"

BOOST_AUTO_TEST_SUITE(CoverTest)

enum Pin : uint8_t {
    UpOutput = 1,
    DownOutput,
    UpInput,
    DownInput,
    StopOutput,
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

    Fixture() {}

    void init(bool isLatching) {
        latching = isLatching;
        initInterface(
            "cover", std::make_unique<Cover>(
                         debug, esp, rtc, UpInput, DownInput, UpOutput,
                         DownOutput, isLatching ? StopOutput : 0, false, false,
                         10, std::vector<PositionSensor>{}, false));
    }

    bool isMoving(uint8_t pin, bool value) {
        bool isStarted = esp.digitalRead(pin) != 0;
        bool result =
            latching ? esp.digitalRead(StopOutput) == 0 && (value || isStarted)
                     : isStarted;
        return result;
    }

    bool isMovingUp() { return isMoving(UpOutput, movingUp); }

    bool isMovingDown() { return isMoving(DownOutput, movingDown); }

    void loop() {
        auto now = esp.millis();
        BOOST_TEST_MESSAGE("Loop begin, time=" << now);
        int delta = now - previousTime;
        const bool upOn = esp.digitalRead(UpOutput) != 0;
        const bool downOn = esp.digitalRead(DownOutput) != 0;

        if (isWorking) {
            if (upOn && downOn) {
                BOOST_FAIL("Should not try to move in both directions.");
            }

            if (latching) {
                if (upOn) {
                    BOOST_TEST_MESSAGE("Start moving up");
                    movingUp = true;
                    movingDown = false;
                } else if (downOn) {
                    BOOST_TEST_MESSAGE("Start moving down");
                    movingUp = false;
                    movingDown = true;
                } else if (esp.digitalRead(StopOutput) != 0) {
                    BOOST_TEST_MESSAGE("Stop");
                    movingUp = false;
                    movingDown = false;
                }
            } else {
                movingUp = upOn;
                movingDown = downOn;
            }
        } else {
            movingUp = false;
            movingDown = false;
        }

        int newPosition = position;

        if (movingUp) {
            newPosition = std::min(maxPosition, position + delta);
        } else if (movingDown) {
            newPosition = std::max(0, position - delta);
        }

        const auto movedUp = newPosition > position;
        const auto movedDown = newPosition < position;

        if (!movedUp) {
            movingUp = false;
        }

        if (!movedDown) {
            movingDown = false;
        }

        BOOST_TEST_MESSAGE(
            "upOn=" << upOn << " downOn=" << downOn << " movingUp=" << movingUp
                    << " movingDown=" << movingDown
                    << " position=" << newPosition << " movedUp=" << movedUp
                    << " movedDown=" << movedDown);

        esp.digitalWrite(UpInput, movedUp);
        esp.digitalWrite(DownInput, movedDown);

        position = newPosition;
        previousTime = now;

        updateInterface();

        BOOST_TEST_MESSAGE("Loop end");
    }

    void open() {
        BOOST_TEST_MESSAGE("Open");
        interface.interface->execute("OPEN");
    }

    void close() {
        BOOST_TEST_MESSAGE("Close");
        interface.interface->execute("CLOSE");
    }

    void stop() {
        BOOST_TEST_MESSAGE("Stop");
        interface.interface->execute("STOP");
    }

    void setPosition(int value) {
        interface.interface->execute(std::to_string(value));
    }

    void loopFor(
        unsigned long time, unsigned long delay,
        std::function<void(unsigned long delta, size_t round)> func) {
        auto beginTime = esp.millis();
        size_t round = 0;
        delayUntil(beginTime + time, delay, [&]() {
            loop();
            auto time = esp.millis() - beginTime;
            ++round;
            BOOST_TEST_CONTEXT(
                "round=" << round << " time=" << time
                         << " position=" << position) {
                BOOST_REQUIRE_NO_THROW(func(time, round));
            }
        });
    }

    void calibrateToPosition(int position, unsigned long delay) {
        setPosition(position);
        loopFor(41000, delay, [](unsigned long, size_t) {});
        if (position <= 10) {
            BOOST_TEST_REQUIRE(getValue(0) == "CLOSED");
        } else {
            BOOST_TEST_REQUIRE(getValue(0) == "OPEN");
        }
        BOOST_TEST_REQUIRE(getValue(1) == std::to_string(position));
        BOOST_TEST_MESSAGE("---- Calibration done. ----");
    }
};

BOOST_FIXTURE_TEST_CASE(NormalMode, Fixture) {
    auto check = [this](const std::string& name, int upValue, int downValue) {
        BOOST_TEST_CONTEXT(name) {
            BOOST_TEST(esp.digitalRead(UpOutput) == upValue);
            BOOST_TEST(esp.digitalRead(DownOutput) == downValue);
            esp.delay(10);
            loop();
            BOOST_TEST(esp.digitalRead(UpOutput) == upValue);
            BOOST_TEST(esp.digitalRead(DownOutput) == downValue);
        }
    };

    position = 5000;
    init(false);
    check("initial state", 0, 0);

    open();
    check("open at init", 1, 0);

    stop();
    check("stop after open", 0, 0);

    close();
    check("close after stop", 0, 1);

    stop();
    check("stop after close", 0, 0);

    open();
    check("open after stop", 1, 0);

    close();
    check("close after open 1", 0, 1);

    open();
    check("open after close", 1, 0);

    close();
    check("close after open 2", 0, 1);
}

BOOST_FIXTURE_TEST_CASE(LatchingMode, Fixture) {
    auto check = [this](
                     const std::string& name, int upValue, int downValue,
                     int stopValue) {
        BOOST_TEST_CONTEXT(name) {
            BOOST_TEST(esp.digitalRead(UpOutput) == upValue);
            BOOST_TEST(esp.digitalRead(DownOutput) == downValue);
            BOOST_TEST(esp.digitalRead(StopOutput) == stopValue);
            esp.delay(10);
            loop();
            BOOST_TEST(esp.digitalRead(UpOutput) == 0);
            BOOST_TEST(esp.digitalRead(DownOutput) == 0);
            BOOST_TEST(esp.digitalRead(StopOutput) == 0);
        }
    };

    position = 5000;
    init(true);
    check("initial state", 0, 0, 1);

    open();
    check("open at init", 1, 0, 0);

    stop();
    check("stop after open", 0, 0, 1);

    close();
    check("close after stop", 0, 1, 0);

    stop();
    check("stop after close", 0, 0, 1);

    open();
    check("open after stop", 1, 0, 0);

    close();
    check("close after open 1", 0, 1, 0);

    open();
    check("open after close", 1, 0, 0);

    close();
    check("close after open 2", 0, 1, 0);
}

namespace {
const auto delays1 = boost::unit_test::data::make({10, 100, 500});
const auto delays2 = boost::unit_test::data::make({10, 100});
const auto latchings = boost::unit_test::data::make({false, true});
const auto params1 = delays1 * latchings;
const auto params2 = delays2 * latchings;
}  // namespace
//

BOOST_DATA_TEST_CASE_F(Fixture, Open, params1, delay, isLatching) {
    init(isLatching);
    esp.delay(10);
    loop();

    open();
    auto func = [&](unsigned long time, size_t round) {
        if (time <= 20 || round == 1) {
            BOOST_TEST(isMovingUp());
            BOOST_TEST(interface.storedValue.size() == 1);
            BOOST_TEST(getValue(0) == "OPENING");
        } else if (time <= 10000) {
            BOOST_TEST(isMovingUp());
            BOOST_TEST(getValue(0) == "OPENING");
            BOOST_TEST(getValue(1) == "1");
        } else {
            BOOST_TEST(!isMovingUp());
            BOOST_TEST(getValue(0) == "OPEN");
            BOOST_TEST(getValue(1) == "100");
        }
    };
    BOOST_REQUIRE_NO_THROW(loopFor(10100, delay, func));
}

BOOST_DATA_TEST_CASE_F(Fixture, Close, params1, delay, isLatching) {
    init(isLatching);
    position = 10000;
    esp.delay(10);
    loop();

    close();
    auto func = [&](unsigned long time, size_t round) {
        if (time <= 20 || round == 1) {
            BOOST_TEST(isMovingDown());
            BOOST_TEST(interface.storedValue.size() == 1);
            BOOST_TEST(getValue(0) == "CLOSING");
        } else if (time <= 10000) {
            BOOST_TEST(isMovingDown());
            BOOST_TEST(getValue(0) == "CLOSING");
            BOOST_TEST(getValue(1) == "99");
        } else {
            BOOST_TEST(!isMovingDown());
            BOOST_TEST(getValue(0) == "CLOSED");
            BOOST_TEST(getValue(1) == "0");
        }
    };
    BOOST_REQUIRE_NO_THROW(loopFor(10100, delay, func));
}

BOOST_DATA_TEST_CASE_F(Fixture, StopWhileOpening, params1, delay, isLatching) {
    init(isLatching);
    esp.delay(10);
    loop();

    open();
    BOOST_REQUIRE_NO_THROW(loopFor(2000, delay, [](unsigned long, size_t) {}));

    stop();
    esp.delay(delay);
    loop();

    BOOST_TEST(!isMovingUp());
    BOOST_TEST(!isMovingDown());
    BOOST_TEST(position == 2000);
}

BOOST_DATA_TEST_CASE_F(Fixture, StopWhileClosing, params1, delay, isLatching) {
    init(isLatching);
    position = 10000;
    esp.delay(10);
    loop();

    close();
    BOOST_REQUIRE_NO_THROW(loopFor(2000, delay, [](unsigned long, size_t) {}));

    stop();
    esp.delay(delay);
    loop();

    BOOST_TEST(!isMovingUp());
    BOOST_TEST(!isMovingDown());
    BOOST_TEST(position == 8000);
}

namespace {
const auto calibrateStartPositions =
    boost::unit_test::data::make({0, 5000, 8000, 10000});
const auto calibrateParams = params1 * calibrateStartPositions;
}  // namespace

BOOST_DATA_TEST_CASE_F(
    Fixture, Calibrate, calibrateParams, delay, isLatching, start) {
    init(isLatching);
    position = start;
    esp.delay(10);
    loop();

    setPosition(40);

    if (start == maxPosition) {
        auto func1 = [&](unsigned long, size_t) {
            BOOST_TEST(isMovingUp());
            BOOST_TEST(position == maxPosition);
            BOOST_TEST(interface.storedValue.size() == 1);
            BOOST_TEST(getValue(0) == "OPENING");
        };
        BOOST_REQUIRE_NO_THROW(loopFor(1000, delay, func1));
    } else {
        auto func1 = [&](unsigned long time, size_t round) {
            BOOST_TEST(isMovingUp());
            BOOST_TEST(getValue(0) == "OPENING");
            if (time <= 20 || round == 1) {
                BOOST_TEST(interface.storedValue.size() == 1);
            } else {
                BOOST_TEST(getValue(1) == "1");
            }
        };
        BOOST_REQUIRE_NO_THROW(loopFor(10000 - start, delay, func1));
    }

    auto funcOpen = [&](unsigned long, size_t) {
        BOOST_TEST(isMovingDown());
        BOOST_TEST(getValue(0) == "OPEN");
        BOOST_TEST(getValue(1) == "100");
    };

    BOOST_REQUIRE_NO_THROW(loopFor(delay, delay, funcOpen));

    auto func2 = [&](unsigned long time, size_t round) {
        BOOST_TEST(isMovingDown());
        BOOST_TEST(getValue(0) == "CLOSING");
        if (time <= 20 || round == 1) {
            BOOST_TEST(getValue(1) == "100");
        } else {
            BOOST_TEST(getValue(1) == "99");
        }
    };
    BOOST_REQUIRE_NO_THROW(loopFor(10000, delay, func2));

    BOOST_REQUIRE_NO_THROW(loopFor(delay, delay, [&](unsigned long, size_t) {
        BOOST_TEST(isMovingUp());
        BOOST_TEST(getValue(0) == "CLOSED");
        BOOST_TEST(getValue(1) == "0");
    }));

    auto func3 = [&](unsigned long time, size_t round) {
        BOOST_TEST(isMovingUp());
        BOOST_TEST(getValue(0) == "OPENING");
        if (time <= 20 || round == 1) {
            BOOST_TEST(getValue(1) == "0");
        } else {
            BOOST_TEST(getValue(1) == "1");
        }
    };
    BOOST_REQUIRE_NO_THROW(loopFor(10000, delay, func3));

    BOOST_REQUIRE_NO_THROW(loopFor(delay, delay, [&](unsigned long, size_t) {
        BOOST_TEST(isMovingDown());
        BOOST_TEST(getValue(0) == "OPEN");
        BOOST_TEST(getValue(1) == "100");
    }));

    auto func4 = [&](unsigned long time, size_t round) {
        BOOST_TEST(isMovingDown());
        BOOST_TEST(getValue(0) == "CLOSING");
        if (time <= 20 || round == 1) {
            BOOST_TEST(getValue(1) == "100");
        } else {
            BOOST_TEST(
                getValue(1) ==
                std::to_string(100 - (time - delay) * 100 / maxPosition));
        }
    };
    BOOST_REQUIRE_NO_THROW(loopFor(6000, delay, func4));

    auto func5 = [&](unsigned long /*time*/, size_t round) {
        BOOST_TEST(!isMovingDown());
        BOOST_TEST(getValue(1) == "40");
        if (round == 1) {
            BOOST_TEST(getValue(0) == "CLOSING");
        } else {
            BOOST_TEST(getValue(0) == "OPEN");
        }
    };
    BOOST_REQUIRE_NO_THROW(loopFor(delay * 3, delay, func5));
    BOOST_TEST(position == 4000 - delay);
    BOOST_TEST(!isMovingUp());
    BOOST_TEST(!isMovingDown());
}

BOOST_DATA_TEST_CASE_F(
    Fixture, OpenAfterCalibrate, params2, delay, isLatching) {
    init(isLatching);
    BOOST_REQUIRE_NO_THROW(calibrateToPosition(60, 100));
    open();
    auto func = [&](unsigned long time, size_t round) {
        if (time <= 20 || round == 1) {
            BOOST_TEST(isMovingUp());
            BOOST_TEST(getValue(0) == "OPENING");
            BOOST_TEST(getValue(1) == "60");
        } else if (time <= 4000) {
            BOOST_TEST(isMovingUp());
            BOOST_TEST(getValue(0) == "OPENING");
            BOOST_TEST(
                getValue(1) ==
                std::to_string(60 + (time - delay) * 100 / maxPosition));
        } else if (time <= 4100) {
            BOOST_TEST(isMovingUp());
            BOOST_TEST(getValue(0) == "OPENING");
            BOOST_TEST(getValue(1) == "99");
        } else {
            BOOST_TEST(!isMovingUp());
            BOOST_TEST(getValue(0) == "OPEN");
            BOOST_TEST(getValue(1) == "100");
        }
    };
    BOOST_REQUIRE_NO_THROW(loopFor(4200, delay, func));
}

BOOST_DATA_TEST_CASE_F(
    Fixture, CloseAfterCalibrate, params2, delay, isLatching) {
    init(isLatching);
    BOOST_REQUIRE_NO_THROW(calibrateToPosition(60, 100));
    close();
    auto func = [&](unsigned long time, size_t round) {
        if (time <= 20 || round == 1) {
            BOOST_TEST(isMovingDown());
            BOOST_TEST(getValue(0) == "CLOSING");
            BOOST_TEST(getValue(1) == "60");
        } else if (time <= 5900) {
            BOOST_TEST(isMovingDown());
            BOOST_TEST(getValue(0) == "CLOSING");
            BOOST_TEST(
                getValue(1) ==
                std::to_string(60 - (time - delay) * 100 / maxPosition));
        } else {
            BOOST_TEST(!isMovingDown());
            BOOST_TEST(getValue(0) == "CLOSED");
            BOOST_TEST(getValue(1) == "0");
        }
    };
    BOOST_REQUIRE_NO_THROW(loopFor(6100, delay, func));
}

BOOST_AUTO_TEST_SUITE_END();
