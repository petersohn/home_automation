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

Cover::Cover(int upMovementPin, int downMovementPin, int upPin, int downPin,
            bool invertInput, bool invertOutput) :
        upMovementPin(upMovementPin),
        downMovementPin(downMovementPin),
        upPin(upPin),
        downPin(downPin),
        invertInput(invertInput),
        invertOutput(invertOutput),
        upTimeId(rtcNext()),
        downTimeId(rtcNext()),
        positionId(rtcNext()) {
}

void Cover::start() {
    pinMode(upMovementPin, INPUT);
    pinMode(downMovementPin, INPUT);
    pinMode(upPin, OUTPUT);
    pinMode(downPin, OUTPUT);

    upTime = rtcGet(upTimeId);
    downTime = rtcGet(downTimeId);
    position = rtcGet(positionId);

    if ((position == 0 && upTime == 0 && downTime == 0) ||
            isMovingUp() || isMovingDown()) {
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
    if (state != State::Opening) {
        stop();
        state = State::Opening;
        digitalWrite(upPin, getActualValue(true, invertOutput));
    }
}

void Cover::beginClosing() {
    if (state != State::Closing) {
        stop();
        state = State::Closing;
        digitalWrite(downPin, getActualValue(true, invertOutput));
    }
}

void Cover::update(Actions action) {
    bool movingUp = isMovingUp();
    bool movingDown = isMovingDown();

    if (movingUp && movingDown) {
        if (position != -1) {
            log("Inconsistent moving state");
            position = -1;
            return;
        }
    }

    int newPosition = position;
    State newState = state;

    if (position != -1) {
        if (movingUp) {
            newPosition = std::min(99UL,
                    moveStartPosition + 100 *
                    (millis() - moveStartTime) / upTime);
        } else if (movingDown) {
            newPosition = std::min(99UL,
                    moveStartPosition + 100 *
                    (millis() - moveStartTime) / upTime);
        }
    }

    switch (state) {
    case State::Idle:
        break;
    case State::Opening:
        if (!movingUp) {
            newPosition = 100;
            stop();
        }
        break;
    case State::Closing:
        if (!movingDown) {
            newPosition = 0;
            stop();
        }
        break;
    }

    if (newPosition != position || stateChanged) {
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

        stateChanged = false;

        if (targetPosition != -1 && state != State::Idle
                && position == targetPosition) {
            targetPosition = -1;
            stop();
        }
    }
}

bool Cover::isMovingUp() const {
    return getActualValue(digitalRead(upMovementPin), invertInput);
}

bool Cover::isMovingDown() const {
    return getActualValue(digitalRead(downMovementPin), invertInput);
}

void Cover::stop() {
    if (state != State::Idle) {
        state = State::Idle;
        stateChanged = true;
    }
    digitalWrite(upPin, getActualValue(false, invertOutput));
    digitalWrite(downPin, getActualValue(false, invertOutput));
}

void Cover::log(const std::string& msg) {
    debugln("Cover " + tools::intToString(upPin) + "." +
            tools::intToString(downPin) + ": " + msg);
}
