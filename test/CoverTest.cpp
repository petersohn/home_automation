#include "InterfaceTestBase.hpp"
#include "common/Cover.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include <algorithm>

BOOST_AUTO_TEST_SUITE(CoverTest)

enum Pin : uint8_t {
    UpOutput, DownOutput, UpInput, DownInput
};

class Fixture : public InterfaceTestBase {
public:
    int maxPosition = 10000;
    int position = 0;
    bool isWorking = true;
    unsigned long previousTime = 0;

    Fixture() {
        initInterface("cover",
            std::make_unique<Cover>(
                debug, esp, rtc, UpInput, DownInput, UpOutput, DownOutput,
                false, false, 10));
    }

    bool isMovingUp() {
        return esp.digitalRead(UpOutput) != 0;
    }

    bool isMovingDown() {
        return esp.digitalRead(DownOutput) != 0;
    }

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

    void open() {
        interface.interface->execute("OPEN");
    }

    void close() {
        interface.interface->execute("CLOSE");
    }

    void stop() {
        interface.interface->execute("STOP");
    }

    void loopFor(unsigned long time, unsigned long delay,
            std::function<void(unsigned long delta)> func) {
        auto beginTime = esp.millis();
        delayUntil(beginTime + time, delay, [&]() {
                loop();
                auto time = esp.millis() - beginTime;
                BOOST_TEST_MESSAGE("time=" << time << " position=" << position);
                BOOST_REQUIRE_NO_THROW(func(time));
            });
    }
};

BOOST_FIXTURE_TEST_CASE(Open, Fixture) {
    esp.delay(10);
    loop();

    open();
    auto func = [&](unsigned long time) {
            if (time <= 20) {
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
    BOOST_REQUIRE_NO_THROW(loopFor(10100, 10, func));
}

BOOST_FIXTURE_TEST_CASE(Close, Fixture) {
    position = 10000;
    esp.delay(10);
    loop();

    close();
    auto func = [&](unsigned long time) {
            if (time <= 20) {
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
    BOOST_REQUIRE_NO_THROW(loopFor(10100, 10, func));
}

BOOST_AUTO_TEST_SUITE_END();
