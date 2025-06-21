#include "Cover.hpp"

#include <cstdint>

#include "../tools/string.hpp"
#include "ArduinoJson.hpp"

using namespace ArduinoJson;

namespace {
bool getActualValue(bool value, bool invert) {
    return invert ? !value : value;
}

constexpr int debounceTime = 20;
constexpr int startTimeout = 1000;
}  // namespace

Cover::Movement::Movement(
    Cover& parent, uint8_t inputPin, uint8_t outputPin, int endPosition,
    int direction, const std::string& directionName)
    : parent(parent)
    , inputPin(inputPin)
    , outputPin(outputPin)
    , beginPosition(100 - endPosition)
    , endPosition(endPosition)
    , direction(direction)
    , debugPrefix(parent.debugPrefix + directionName + ": ") {
    parent.esp.pinMode(inputPin, GpioMode::input);
    parent.esp.pinMode(outputPin, GpioMode::output);
    size_t timeCount = 1;
    if (parent.hasPositionSensors()) {
        timeCount = parent.positionSensors.size() - 1;
    } else {
        moveTimeIndex = 0;
    }
    moveTimes.reserve(timeCount);
    for (size_t i = 0; i < timeCount; ++i) {
        const auto id = parent.rtc.next();
        moveTimes.emplace_back(MoveTime{id, parent.rtc.get(id)});
    }
}

void Cover::Movement::start() {
    parent.resetStop();
    log("Start");
    parent.setOutput(outputPin, true);
    startTriggered = true;
    if (!isStarted()) {
        startedTime = parent.esp.millis();
    }
}

void Cover::Movement::stop() {
    log("stop");
    resetStart();
    if (parent.isLatching()) {
        stopTriggered = true;
        parent.setOutput(parent.stopPin, true);
    }
    resetStarted();
}

void Cover::Movement::resetStarted() {
    if (startedTime != 0) {
        startedTime = 0;
        parent.stateChanged = true;
    }
}

void Cover::Movement::resetStart() {
    parent.setOutput(outputPin, false);
    startTriggered = false;
}

void Cover::Movement::handleStopped() {
    if (!parent.isLatching() || startTriggered) {
        stop();
    } else {
        resetStarted();
    }
}

void Cover::Movement::log(const std::string& msg) {
    parent.debug << debugPrefix << msg << std::endl;
}

bool Cover::Movement::isMoving() const {
    return getActualValue(parent.esp.digitalRead(inputPin), parent.invertInput);
}

bool Cover::Movement::isStarted() const {
    return startedTime != 0;
}

bool Cover::Movement::isReallyMoving() const {
    return moveStartPosition != -2;
}

bool Cover::Movement::shouldResetStop() const {
    return stopTriggered && !isMoving();
}

void Cover::Movement::resetStop() {
    stopTriggered = false;
}

int Cover::Movement::update() {
    int newPosition = parent.position;
    auto now = parent.esp.millis();
    bool moving = isMoving();

    if (parent.isLatching()) {
        if (moving && startTriggered) {
            log("Reset start");
            resetStart();
        }
    }

    if (parent.activePositionSensor >= 0) {
        calculateMoveTimeIfNeeded();
        moveStartTime = 0;
        moveStartPosition = -2;
        return parent.position;
    }

    if (parent.position != 0 && moveTimeIndex < 0) {
        for (size_t i = 0; i < parent.positionSensors.size(); ++i) {
            size_t j = parent.positionSensors.size() - 1 - i;
            if (parent.position >= parent.positionSensors[j].position) {
                if (j < parent.positionSensors.size() - 1) {
                    log("Found position index: " + tools::intToString(j));
                    moveTimeIndex = j;
                    calculateBeginAndEndPosition();
                }
                break;
            }
        }
    }

    log("moveStartTime=" + tools::intToString(moveStartTime));

    if (moving) {
        didNotStartCount = 0;

        const auto paps = parent.previouslyActivePositionSensor;
        if (paps >= 0) {
            log("Just left position sensor " + tools::intToString(paps));
            moveTimeIndex = direction > 0 ? paps : paps - 1;
            if (moveTimeIndex >= static_cast<int>(moveTimes.size())) {
                moveTimeIndex = -1;
            }
            if (moveTimeIndex >= 0) {
                moveStartTime = now;
                calculateBeginAndEndPosition();
                newPosition = beginPosition + direction;
                moveStartPosition = beginPosition;
            }
        } else {
            if (moveStartTime == 0) {
                moveStartTime = now;
            } else if (
                !isReallyMoving() && now - moveStartTime >= debounceTime) {
                moveStartPosition = parent.position;
                log("Started moving");
            }

            if (parent.position == endPosition) {
                newPosition = endPosition - direction;
                //} else if (parent.position == beginPosition) {
                //    newPosition = beginPosition + direction;
            }
        }
    }

    if (moveTimeIndex >= 0 && isReallyMoving()) {
        const auto& moveTime = moveTimes[moveTimeIndex].time;
        if (moving) {
            if (parent.position != -1 && moveTime != 0) {
                const int a =
                    (endPosition - beginPosition) * (now - moveStartTime);
                const auto d =
                    static_cast<int>(static_cast<double>(a) / moveTime);
                newPosition = moveStartPosition + d;
                if (direction * newPosition >= endPosition) {
                    newPosition = endPosition - direction;
                }
            } else {
                newPosition = beginPosition + direction;
            }
        } else if (isStarted()) {
            if (parent.hasPositionSensors()) {
                log("Stopped.");
            } else {
                log("End position reached.");
                newPosition = endPosition;
                calculateMoveTimeIfNeeded();
            }
        }
    } else if (!moving && isStarted() && now - startedTime > startTimeout) {
        ++didNotStartCount;
        if (parent.hasPositionSensors()) {
            log("Did not start.");
        } else {
            log("Was at end position.");
            newPosition = endPosition;
        }
    }

    if (!moving) {
        if (isReallyMoving()) {
            log("Stopped moving");
        }

        moveStartTime = 0;
        moveStartPosition = -2;

        if (isStarted()) {
            handleStopped();
        }
    }

    return newPosition;
}

void Cover::Movement::calculateBeginAndEndPosition() {
    if (moveTimeIndex < 0) {
        return;
    }

    if (direction > 0) {
        beginPosition = parent.positionSensors[moveTimeIndex].position;
        endPosition = parent.positionSensors[moveTimeIndex + 1].position;
    } else {
        beginPosition = parent.positionSensors[moveTimeIndex + 1].position;
        endPosition = parent.positionSensors[moveTimeIndex].position;
    }
}

void Cover::Movement::calculateMoveTimeIfNeeded() {
    if (moveTimeIndex < 0) {
        return;
    }

    auto& moveTime = moveTimes[moveTimeIndex];
    if (moveStartPosition == beginPosition) {
        moveTime.time = parent.esp.millis() - moveStartTime;
        parent.rtc.set(moveTime.rtcId, moveTime.time);
        log("Move time: " + tools::intToString(moveTime.time));
    }
}

Cover::Cover(
    std::ostream& debug, EspApi& esp, Rtc& rtc, uint8_t upMovementPin,
    uint8_t downMovementPin, uint8_t upPin, uint8_t downPin, uint8_t stopPin,
    bool invertInput, bool invertOutput, int closedPosition,
    std::vector<PositionSensor> positionSensors, bool invertPositionSensors)
    : debug(debug)
    , esp(esp)
    , rtc(rtc)
    , debugPrefix(
          "Cover " + tools::intToString(upPin) + "." +
          tools::intToString(downPin) + ": ")
    , positionSensors(std::move(positionSensors))
    , up(*this, upMovementPin, upPin, 100, 1, "up")
    , down(*this, downMovementPin, downPin, 0, -1, "down")
    , stopPin(stopPin)
    , invertInput(invertInput)
    , invertOutput(invertOutput)
    , closedPosition(closedPosition)
    , invertPositionSensors(invertPositionSensors)
    , positionId(rtc.next()) {
    if (this->positionSensors.size() == 1) {
        debug << "Invalid position sensors: there should be zero or at least 2."
              << std::endl;
        this->positionSensors.clear();
    }

    std::sort(
        this->positionSensors.begin(), this->positionSensors.end(),
        [](const PositionSensor& lhs, const PositionSensor& rhs) {
        return lhs.position < rhs.position;
    });

    position = rtc.get(positionId) - 1;
    log("Initial position: " + tools::intToString(position));
    stop();
}

bool Cover::isLatching() const {
    return stopPin != 0;
}

bool Cover::hasPositionSensors() const {
    return !positionSensors.empty();
}

void Cover::start() {
    stateChanged = true;
}

void Cover::execute(const std::string& command) {
    if (command == "STOP") {
        targetPosition = -1;
        stop();
    } else if (command == "OPEN") {
        targetPosition = -1;
        beginOpening();
    } else if (command == "CLOSE") {
        targetPosition = -1;
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

void Cover::setPosition(int value) {
    if (value < 0 || value > 100) {
        log("Position out of range: " + tools::intToString(value));
        return;
    }

    if (position == -1) {
        log("Position is not known, calibrating.");
    }

    targetPosition = value;

    if (value == 0 || value < position) {
        beginClosing();
    } else if (value == 100 || value > position) {
        beginOpening();
    } else {
        stop();
    }
}

void Cover::beginMoving(Movement& direction, Movement& reverse) {
    if (!direction.isStarted()) {
        if (isLatching()) {
            reverse.resetStarted();
        } else {
            reverse.stop();
        }
        direction.start();
        stateChanged = true;
    }
}

void Cover::beginOpening() {
    beginMoving(up, down);
}

void Cover::beginClosing() {
    beginMoving(down, up);
}

void Cover::resetStop() {
    if (!isLatching()) {
        return;
    }

    setOutput(stopPin, false);
    up.resetStop();
    down.resetStop();
}

void Cover::update(Actions action) {
    int newPositionSensor = -1;
    for (size_t i = 0; i < positionSensors.size(); ++i) {
        if (getActualValue(
                esp.digitalRead(positionSensors[i].pin) != 0,
                invertPositionSensors)) {
            newPositionSensor = i;
            break;
        }
    }

    previouslyActivePositionSensor =
        newPositionSensor == -1 ? activePositionSensor : -1;
    activePositionSensor = newPositionSensor;

    int newPositionUp = up.update();
    int newPositionDown = down.update();
    int newPosition = position;
    if (newPositionUp != position && newPositionDown != position) {
        log("Inconsistent moving state.");
        newPosition = -1;
        stop();
    } else if (newPositionUp != position) {
        newPosition = newPositionUp;
    } else {
        newPosition = newPositionDown;
    }

    if (activePositionSensor != -1) {
        newPosition = positionSensors[activePositionSensor].position;
    }

    if (up.shouldResetStop() && down.shouldResetStop()) {
        log("Reset stop");
        resetStop();
    }

    if (newPosition != position || stateChanged) {
        position = newPosition;
        rtc.set(positionId, position + 1);
        std::string stateName;

        if (up.isStarted()) {
            stateName = "OPENING";
        } else if (down.isStarted()) {
            stateName = "CLOSING";
        } else if (position <= closedPosition) {
            stateName = "CLOSED";
        } else {
            stateName = "OPEN";
        }

        log("state=" + stateName + " position=" + tools::intToString(position));

        std::vector<std::string> values{std::move(stateName)};
        if (position != -1) {
            values.push_back(tools::intToString(position));
        }
        action.fire(values);

        stateChanged = false;
    }

    if (targetPosition != -1) {
        if (position == targetPosition) {
            targetPosition = -1;
            stop();
        } else if (!up.isStarted() && !down.isStarted()) {
            if (up.getDidNotStartCount() < 2 &&
                down.getDidNotStartCount() < 2) {
                setPosition(targetPosition);
            } else {
                targetPosition = -1;
                up.resetDidNotStartCount();
                down.resetDidNotStartCount();
            }
        }
    }
}

void Cover::setOutput(uint8_t pin, bool value) {
    esp.digitalWrite(pin, getActualValue(value, invertOutput));
}

void Cover::stop() {
    up.stop();
    down.stop();
}

void Cover::log(const std::string& msg) {
    debug << debugPrefix << msg << std::endl;
}
