#include "Cover.hpp"

#include "ArduinoJson.hpp"
#include "debug.hpp"
#include "rtc.hpp"
#include "tools/string.hpp"

#include <Arduino.h>

namespace {
bool getActualValue(bool value, bool invert) {
    return invert ? !value : value;
}
}

Cover::Cover(int movementPin, int upPin, int downPin,
        bool invertMovement, bool invertUpDown) :
        movementPin(movementPin),
        upPin(upPin),
        downPin(downPin),
        invertMovement(invertMovement),
        invertUpDown(invertUpDown),
        upTimeId(rtcNext()),
        downTimeId(rtcNext()),
        positionId(rtcNext()) {
}

void Cover::start() {
    pinMode(movementPin, INPUT);
    pinMode(upPin, OUTPUT);
    pinMode(downPin, OUTPUT);

    upTime = rtcGet(upTimeId);
    downTime = rtcGet(downTimeId);
    position = rtcGet(positionId);

    if ((position == 0 && upTime == 0 && downTime == 0) || isMoving()) {
        log("Cannot determine initial position");
        position = -1;
    } else {
        debug("Initial position: ");
        log("Initial position: " + tools::intToString(position));
    }

    stop();
}

void Cover::execute(const std::string& command) {
    if (command == "STOP") {
        stop();
    } else if (command == "OPEN") {
        beginOpening();
    } else if (command == "CLOSE") {
        beginClosing();
    } else {
        StaticJsonBuffer<20> buf;
        auto json = buf.parse(command);
        if (!json.is<int>()) {
            log("Invalid command: " + command);
            return;
        }
        setPosition(json.as<int>());
    }
}

void Cover::setPosition(int value)
{
    if (value < 0 || value > 100) {
        log("Position out of range: " + tools::intToString(value));
        return;
    }

    if (position == -1 && value != 0 && value != 100) {
        log("Position is not known");
        return;
    }

    targetPosition = value;

    if (value == 100 || value > position) {
        beginOpening();
    } else if (value == 0 || value < position) {
        beginClosing();
    } else {
        stop();
    }
}

void Cover::beginOpening() {
    if (state != State::BeginOpening && state != State::Opening) {
        stop();
        state = State::BeginOpening;
        digitalWrite(upPin, getActualValue(true, invertUpDown));
    }
}

void Cover::beginClosing() {
    if (state != State::BeginClosing && state != State::Closing) {
        stop();
        state = State::BeginClosing;
        digitalWrite(downPin, getActualValue(true, invertUpDown));
    }
}

void Cover::update(Actions action) {
    bool moving = isMoving();
    auto now = millis();
    State newState = state;
    int newPosition = position;
    switch (state) {
    case State::Stopping:
        if (!moving) {
            newState = State::Idle;
        }
        break;
    case State::Idle:
        if (moving) {
            newPosition = -1;
        }
        break;
    case State::BeginOpening:
        if (moving) {
            moveStartTime = now;
            moveStartPosition = position;
            newState = State::Opening;
        }
        break;
    case State::Opening:
        if (!moving) {
            log("Reached upper end");
            newPosition = 100;
            newState = State::Idle;
            if (moveStartPosition == 0) {
                upTime = now - moveStartTime;
            }
        } else if (upTime != 0 && moveStartPosition != -1) {
            newPosition = std::min(99UL,
                    moveStartPosition + 100 * (now - moveStartTime) / upTime);
        }
        break;
    case State::BeginClosing:
        if (moving) {
            moveStartTime = now;
            moveStartPosition = position;
            newState = State::Closing;
        }
        break;
    case State::Closing:
        if (!moving) {
            log("Reached lower end");
            newPosition = 0;
            newState = State::Idle;
            if (moveStartPosition == 100) {
                downTime = now - moveStartTime;
            }
        } else if (downTime != 0 && moveStartPosition != -1) {
            newPosition = std::max(1UL,
                    moveStartPosition - 100 * (now - moveStartTime) / downTime);
        }
        break;
    }

    if (newState != state || newPosition != position) {
        state = newState;
        position = newPosition;
        log("state=" + tools::intToString(static_cast<int>(state)) +
                " position=" + tools::intToString(position));
        std::string stateName;

        switch (state) {
        case State::Idle:
            stateName = "IDLE";
            break;
        case State::Opening:
            stateName = "OPENING";
            break;
        case State::Closing:
            stateName = "CLOSING";
            break;
        default:
            break;
        }

        if (!stateName.empty()) {
            std::vector<std::string> values{std::move(stateName)};
            if (position != -1) {
                values.push_back(tools::intToString(position));
            }
        }

        if (targetPosition != -1 && moving && position == targetPosition) {
            targetPosition = -1;
            stop();
        }
    }
}

bool Cover::isMoving() const {
    return getActualValue(digitalRead(movementPin), invertMovement);
}

void Cover::stop() {
    if (isMoving()) {
        state = State::Stopping;
        digitalWrite(upPin, getActualValue(false, invertUpDown));
        digitalWrite(downPin, getActualValue(false, invertUpDown));
    }
}

void Cover::log(const std::string& msg) {
    debugln("Cover " + tools::intToString(movementPin) + ": " + msg);
}
