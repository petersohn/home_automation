#include "Cover.hpp"

#include <cstdint>

#include "../tools/fromString.hpp"
#include "../tools/string.hpp"

namespace {
bool getActualValue(bool value, bool invert) {
    return invert ? !value : value;
}

constexpr int debounceTime = 20;
constexpr int startTimeout = 1000;
constexpr int papsNoChange = -2;
constexpr int noPosition = -1;
constexpr int noPositionSensor = -1;
constexpr int mspNotMoving = -2;

constexpr int upDirection = 1;
constexpr int downDirection = -1;
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
    this->parent.esp.pinMode(inputPin, GpioMode::input);
    this->parent.esp.pinMode(outputPin, GpioMode::output);
    this->parent.setOutput(this->outputPin, false);
    size_t timeCount = 1;
    if (this->parent.hasPositionSensors()) {
        timeCount = this->parent.positionSensors.size() - 1;
    }

    if (timeCount == 1) {
        this->moveTimeIndex = 0;
    }

    this->moveTimes.reserve(timeCount);
    for (size_t i = 0; i < timeCount; ++i) {
        const auto id = this->parent.rtc.next();
        this->moveTimes.emplace_back(MoveTime{id, this->parent.rtc.get(id)});
    }
}

void Cover::Movement::start() {
    this->parent.stopper.reset();
    this->log("Start");
    this->parent.setOutput(this->outputPin, true);
    this->startTriggered = true;
    if (!this->isStarted()) {
        this->startedTime = this->parent.esp.millis();
    }
}

void Cover::Movement::stop() {
    this->log("stop");
    this->resetStart();
    this->resetStarted();
}

void Cover::Movement::resetStarted() {
    if (this->startedTime != 0) {
        this->startedTime = 0;
        this->parent.stateChanged = true;
    }
}

void Cover::Movement::resetStart() {
    this->parent.setOutput(this->outputPin, false);
    this->startTriggered = false;
}

void Cover::Movement::handleStopped() {
    if (!this->parent.stopper.isLatching() || this->startTriggered) {
        this->stop();
    } else {
        this->resetStarted();
    }
}

void Cover::Movement::log(const std::string& msg) {
    this->parent.debug << this->debugPrefix << msg << std::endl;
}

bool Cover::Movement::isMoving() const {
    return getActualValue(
        this->parent.esp.digitalRead(this->inputPin), this->parent.invertInput);
}

bool Cover::Movement::isStarted() const {
    return this->startedTime != 0;
}

bool Cover::Movement::isReallyMoving() const {
    return this->moveStartPosition != mspNotMoving;
}

int Cover::Movement::update() {
    int newPosition = this->parent.position;
    auto now = this->parent.esp.millis();
    bool moving = this->isMoving();

    if (this->parent.stopper.isLatching()) {
        if (moving && this->startTriggered) {
            this->log("Reset start");
            this->resetStart();
        }
    }

    const auto paps = this->parent.previouslyActivePositionSensor;
    const bool hasActivePositionSensor = this->parent.activePositionSensor >= 0;
    if (hasActivePositionSensor) {
        if (paps == noPositionSensor) {
            this->calculateMoveTimeIfNeeded();
        }
        this->moveStartTime = 0;
    } else {
        if (this->parent.position != noPosition && this->moveTimeIndex < 0) {
            for (size_t i = 0; i < this->parent.positionSensors.size(); ++i) {
                size_t j = this->parent.positionSensors.size() - 1 - i;
                if (this->parent.position >=
                    this->parent.positionSensors[j].position) {
                    if (j < this->parent.positionSensors.size() - 1) {
                        this->log(
                            "Found position index: " + tools::intToString(j));
                        this->moveTimeIndex = j;
                        this->calculateBeginAndEndPosition();
                    }
                    break;
                }
            }
        }

        if (moving) {
            this->didNotStartCount = 0;

            if (paps >= 0) {
                this->log(
                    "Just left position sensor " + tools::intToString(paps));
                this->moveTimeIndex = this->direction > 0 ? paps : paps - 1;
                if (this->moveTimeIndex >=
                    static_cast<int>(this->moveTimes.size())) {
                    this->moveTimeIndex = noPositionSensor;
                }
                if (this->moveTimeIndex >= 0) {
                    this->moveStartTime = now;
                    this->calculateBeginAndEndPosition();
                    newPosition = this->beginPosition + this->direction;
                    this->moveStartPosition = this->beginPosition;
                }
            } else {
                if (this->moveStartTime == 0) {
                    this->moveStartTime = now;
                } else if (
                    !this->isReallyMoving() &&
                    now - this->moveStartTime >= debounceTime) {
                    this->moveStartPosition = this->parent.position;
                    this->log("Started moving");
                }

                if (this->parent.position == this->endPosition) {
                    newPosition = this->endPosition - this->direction;
                }
            }
        }
    }

    if (this->isReallyMoving()) {
        // Stop debounce tracking
        if (!moving) {
            if (this->moveStopTime == 0) {
                this->moveStopTime = now;
            }
        } else {
            this->moveStopTime = 0;
        }

        if (moving) {
            if (!hasActivePositionSensor && this->moveTimeIndex >= 0) {
                const auto& moveTime =
                    this->moveTimes[this->moveTimeIndex].time;
                if (this->parent.position != noPosition && moveTime != 0) {
                    const int a = (this->endPosition - this->beginPosition) *
                                  (now - this->moveStartTime);
                    const auto d =
                        static_cast<int>(static_cast<double>(a) / moveTime);
                    newPosition = this->moveStartPosition + d;
                    if (this->direction * newPosition >= this->endPosition) {
                        newPosition = this->endPosition - this->direction;
                    }
                } else {
                    newPosition = this->beginPosition + this->direction;
                }
            }
        } else if (
            this->isStarted() && this->moveStopTime != 0 &&
            now - this->moveStopTime >= debounceTime) {
            if (!this->parent.hasPositionSensors()) {
                this->log("End position reached.");
                newPosition = this->endPosition;
                this->calculateMoveTimeIfNeeded();
            }
            this->handleStopped();
        }

        if (!moving && this->moveStopTime != 0 &&
            now - this->moveStopTime >= debounceTime) {
            this->log("Stopped moving");
            this->moveStartTime = 0;
            this->moveStartPosition = mspNotMoving;
        }
    } else {
        this->moveStopTime = 0;
        if (!moving && this->isStarted() &&
            now - this->startedTime > startTimeout) {
            ++this->didNotStartCount;
            if (this->parent.hasPositionSensors()) {
                this->log("Did not start.");
            } else {
                this->log("Was at end position.");
                newPosition = this->endPosition;
            }
            this->handleStopped();
        }

        if (!moving) {
            this->moveStartTime = 0;
            this->moveStartPosition = mspNotMoving;
        }
    }

    return newPosition;
}

void Cover::Movement::calculateBeginAndEndPosition() {
    if (this->moveTimeIndex < 0) {
        return;
    }

    if (this->direction > 0) {
        this->beginPosition =
            this->parent.positionSensors[this->moveTimeIndex].position;
        this->endPosition =
            this->parent.positionSensors[this->moveTimeIndex + 1].position;
    } else {
        this->beginPosition =
            this->parent.positionSensors[this->moveTimeIndex + 1].position;
        this->endPosition =
            this->parent.positionSensors[this->moveTimeIndex].position;
    }
}

void Cover::Movement::calculateMoveTimeIfNeeded() {
    if (this->moveTimeIndex < 0) {
        return;
    }

    auto& moveTime = this->moveTimes[this->moveTimeIndex];
    if (this->moveStartPosition == this->beginPosition) {
        moveTime.time = this->parent.esp.millis() - this->moveStartTime;
        this->parent.rtc.set(moveTime.rtcId, moveTime.time);
        this->log("Move time: " + tools::intToString(moveTime.time));
    }
}

Cover::Stop::Stop(Cover& parent, uint8_t pin, bool latching)
    : parent(parent), pin(pin), latching(latching) {
    if (this->isLatching()) {
        this->parent.esp.pinMode(this->pin, GpioMode::output);
        this->stop();
    }
}

void Cover::Stop::stop() {
    if (!this->isLatching()) {
        return;
    }

    this->parent.log("stop");
    this->triggered = true;
    this->parent.setOutput(this->pin, true);
}

void Cover::Stop::reset() {
    if (!this->isLatching()) {
        return;
    }

    this->parent.log("Reset stop");
    this->parent.setOutput(this->pin, false);
    this->triggered = false;
}

bool Cover::Stop::isTriggered() const {
    return this->triggered;
}

bool Cover::Stop::isLatching() const {
    return this->latching;
}

Cover::Cover(
    std::ostream& debug, EspApi& esp, Rtc& rtc, uint8_t upMovementPin,
    uint8_t downMovementPin, uint8_t upPin, uint8_t downPin, uint8_t stopPin,
    bool latching, bool invertInput, bool invertOutput, int closedPosition,
    std::vector<PositionSensor> positionSensors, bool invertPositionSensors)
    : debug(debug)
    , esp(esp)
    , rtc(rtc)
    , debugPrefix(
          "Cover " + tools::intToString(upPin) + "." +
          tools::intToString(downPin) + ": ")
    , positionSensors(std::move(positionSensors))
    , stopper(*this, stopPin, latching)
    , up(*this, upMovementPin, upPin, 100, upDirection, "up")
    , down(*this, downMovementPin, downPin, 0, downDirection, "down")
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

    if (!this->positionSensors.empty() &&
        (this->positionSensors.front().position != 0 ||
         this->positionSensors.back().position != 100)) {
        debug << "Invalid position sensors: positions should go from 0 to 100."
              << std::endl;
        this->positionSensors.clear();
    }

    this->position = this->rtc.get(this->positionId) - 1;
    this->log("Initial position: " + tools::intToString(this->position));
    this->stop();
}

bool Cover::hasPositionSensors() const {
    return !this->positionSensors.empty();
}

void Cover::start() {
    this->stateChanged = true;
}

void Cover::execute(const std::string& command) {
    if (command == "STOP") {
        this->targetPosition = noPosition;
        this->stop();
    } else if (command == "OPEN") {
        this->targetPosition = noPosition;
        this->beginOpening();
    } else if (command == "CLOSE") {
        this->targetPosition = noPosition;
        this->beginClosing();
    } else {
        auto pos = tools::fromString<int>(command);
        if (!pos.has_value()) {
            this->log("Invalid command: " + command);
            return;
        }
        this->setPosition(*pos);
    }
}

void Cover::setPosition(int value) {
    if (value < 0 || value > 100) {
        this->log("Position out of range: " + tools::intToString(value));
        return;
    }

    if (this->position == noPosition) {
        this->log("Position is not known, calibrating.");
    }

    this->targetPosition = value;

    if (value == 0 || value < this->position) {
        this->beginClosing();
    } else if (value == 100 || value > this->position) {
        this->beginOpening();
    } else {
        this->stop();
    }
}

void Cover::beginMoving(Movement& direction, Movement& reverse) {
    if (!direction.isStarted()) {
        reverse.stop();
        direction.start();
        this->stateChanged = true;
    }
}

void Cover::beginOpening() {
    this->beginMoving(this->up, this->down);
}

void Cover::beginClosing() {
    this->beginMoving(this->down, this->up);
}

void Cover::update(Actions action) {
    int newPositionSensor = noPositionSensor;
    for (size_t i = 0; i < this->positionSensors.size(); ++i) {
        if (getActualValue(
                getActualValue(
                    this->esp.digitalRead(this->positionSensors[i].pin),
                    this->positionSensors[i].invert) != 0,
                this->invertPositionSensors)) {
            newPositionSensor = i;
            break;
        }
    }

    if (newPositionSensor != this->activePositionSensor) {
        this->previouslyActivePositionSensor = this->activePositionSensor;
        if (newPositionSensor >= 0) {
            this->log(
                "Position sensor activated: " +
                tools::intToString(
                    this->positionSensors[newPositionSensor].position));
        } else {
            this->log("Position sensor deactivated");
        }
        this->activePositionSensor = newPositionSensor;
    } else {
        this->previouslyActivePositionSensor = papsNoChange;
    }

    int newPositionUp = this->up.update();
    int newPositionDown = this->down.update();
    int newPosition = this->position;
    if (newPositionUp != this->position && newPositionDown != this->position) {
        this->log("Inconsistent moving state.");
        newPosition = noPosition;
        this->stop();
    } else if (newPositionUp != this->position) {
        newPosition = newPositionUp;
    } else {
        newPosition = newPositionDown;
    }

    int movementDirection = 0;
    if (this->up.isMoving()) {
        movementDirection = upDirection;
    } else if (this->down.isMoving()) {
        movementDirection = downDirection;
    }

    if (this->previousMovementDirection != movementDirection) {
        this->previousMovementDirection = movementDirection;
        this->stateChanged = true;
    }

    if (this->activePositionSensor != noPositionSensor) {
        newPosition =
            this->positionSensors[this->activePositionSensor].position;
    }

    if (this->stopper.isTriggered() && !this->up.isMoving() &&
        !this->down.isMoving()) {
        this->stopper.reset();
    }

    if (newPosition != this->position || this->stateChanged) {
        this->position = newPosition;
        this->rtc.set(this->positionId, this->position + 1);
        std::string stateName;

        if (movementDirection == upDirection) {
            stateName = "OPENING";
        } else if (movementDirection == downDirection) {
            stateName = "CLOSING";
        } else if (this->position <= this->closedPosition) {
            stateName = "CLOSED";
        } else {
            stateName = "OPEN";
        }

        this->log(
            "state=" + stateName +
            " position=" + tools::intToString(this->position));

        std::vector<std::string> values{std::move(stateName)};
        if (this->position != noPosition) {
            values.push_back(tools::intToString(this->position));
        }
        action.fire(values);

        this->stateChanged = false;
    }

    if (this->targetPosition != noPosition) {
        if (this->position == this->targetPosition) {
            this->targetPosition = noPosition;
            this->stop();
        } else if (!this->up.isStarted() && !this->down.isStarted()) {
            if (this->up.getDidNotStartCount() < 2 &&
                this->down.getDidNotStartCount() < 2) {
                this->setPosition(this->targetPosition);
            } else {
                this->targetPosition = noPosition;
                this->up.resetDidNotStartCount();
                this->down.resetDidNotStartCount();
            }
        }
    }
}

void Cover::setOutput(uint8_t pin, bool value) {
    this->esp.digitalWrite(pin, getActualValue(value, this->invertOutput));
}

void Cover::stop() {
    this->up.stop();
    this->down.stop();
    this->stopper.stop();
}

void Cover::log(const std::string& msg) {
    this->debug << this->debugPrefix << msg << std::endl;
}
