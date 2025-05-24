#include <algorithm>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

#include "InterfaceTestBase.hpp"
#include "common/Cover.hpp"

BOOST_AUTO_TEST_SUITE(CoverTest)

enum Pin : uint8_t { UpOutput = 1, DownOutput, UpInput, DownInput };

class Fixture : public InterfaceTestBase {
public:
    int maxPosition = 10000;
    int position = 0;
    bool isWorking = true;
    unsigned long previousTime = 0;

    Fixture() {
        initInterface(
            "cover", std::make_unique<Cover>(
                         debug, esp, rtc, UpInput, DownInput, UpOutput,
                         DownOutput, false, false, 10));
    }

    bool isMovingUp() { return esp.digitalRead(UpOutput) != 0; }

    bool isMovingDown() { return esp.digitalRead(DownOutput) != 0; }

    void loop() {
        auto now = esp.millis();
        int delta = now - previousTime;
        auto movingUp = isWorking && isMovingUp();
        auto movingDown = isWorking && isMovingDown();
        if (movingUp && movingDown) {
            BOOST_FAIL("Should not try to move in both directions.");
        }

        int newPosition = position;

        if (movingUp) {
            newPosition = std::min(maxPosition, position + delta);
        } else if (movingDown) {
            newPosition = std::max(0, position - delta);
        }

        esp.digitalWrite(UpInput, newPosition > position);
        esp.digitalWrite(DownInput, newPosition < position);

        position = newPosition;
        previousTime = now;

        updateInterface();
    }

    void open() { interface.interface->execute("OPEN"); }

    void close() { interface.interface->execute("CLOSE"); }

    void stop() { interface.interface->execute("STOP"); }

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

namespace {
auto delays1 = boost::unit_test::data::make({1, 10, 100, 500});
auto delays2 = boost::unit_test::data::make({1, 10, 100});
}  // namespace

BOOST_DATA_TEST_CASE_F(Fixture, Open, delays1, delay) {
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

BOOST_DATA_TEST_CASE_F(Fixture, Close, delays1, delay) {
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

BOOST_DATA_TEST_CASE_F(
    Fixture, Calibrate,
    delays1* boost::unit_test::data::make({0, 5000, 8000, 10000}), delay,
    start) {
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

    BOOST_REQUIRE_NO_THROW(loopFor(delay, delay, [&](unsigned long, size_t) {
        BOOST_TEST(isMovingDown());
        BOOST_TEST(getValue(0) == "OPEN");
        BOOST_TEST(getValue(1) == "100");
    }));

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

BOOST_DATA_TEST_CASE_F(Fixture, OpenAfterCalibrate, delays2, delay) {
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

BOOST_DATA_TEST_CASE_F(Fixture, CloseAfterCalibrate, delays2, delay) {
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
