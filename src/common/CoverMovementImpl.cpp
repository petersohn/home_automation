#include "CoverMovementImpl.hpp"

#include "../tools/string.hpp"

namespace {
bool getActualValue(bool value, bool invert) {
    return invert ? !value : value;
}

constexpr int debounceTime = 20;
constexpr int startTimeout = 1000;
constexpr int noPositionSensor = -1;
constexpr int noPosition = -1;
constexpr int mspNotMoving = -2;
}  // namespace

CoverMovementImpl::CoverMovementImpl(
    CoverState& context, CoverStop& stopper, uint8_t inputPin,
    uint8_t outputPin, int endPosition, int direction,
    std::string directionName)
    : state(context)
    , stopper(stopper)
    , inputPin(inputPin)
    , outputPin(outputPin)
    , beginPosition(100 - endPosition)
    , endPosition(endPosition)
    , direction(direction)
    , debugPrefix(this->state.debugPrefix + directionName + ": ") {
    this->state.esp.pinMode(inputPin, GpioMode::input);
    this->state.esp.pinMode(outputPin, GpioMode::output);
    this->state.esp.digitalWrite(
        this->outputPin, this->state.invertOutput ? 1 : 0);
    size_t timeCount = 1;
    if (this->state.hasPositionSensors()) {
        timeCount = this->state.positionSensors.size() - 1;
    }

    if (timeCount == 1) {
        this->moveTimeIndex = 0;
    }

    this->moveTimes.reserve(timeCount);
    for (size_t i = 0; i < timeCount; ++i) {
        const auto id = this->state.rtc.next();
        this->moveTimes.emplace_back(MoveTime{id, this->state.rtc.get(id)});
    }
}

void CoverMovementImpl::start() {
    this->stopper.reset();
    this->log("Start");
    this->state.esp.digitalWrite(
        this->outputPin, this->state.invertOutput ? 0 : 1);
    this->startTriggered = true;
    if (!this->isStarted()) {
        this->startedTime = this->state.esp.millis();
    }
}

void CoverMovementImpl::stop() {
    this->log("stop");
    this->resetStart();
    this->resetStarted();
}

void CoverMovementImpl::resetStarted() {
    if (this->startedTime != 0) {
        this->startedTime = 0;
        this->state.stateChanged = true;
    }
}

void CoverMovementImpl::resetStart() {
    this->state.esp.digitalWrite(
        this->outputPin, this->state.invertOutput ? 1 : 0);
    this->startTriggered = false;
}

void CoverMovementImpl::handleStopped() {
    if (!this->state.latching || this->startTriggered) {
        this->stop();
    } else {
        this->resetStarted();
    }
}

void CoverMovementImpl::log(const std::string& msg) {
    this->state.debug << this->debugPrefix << msg << std::endl;
}

bool CoverMovementImpl::isMoving() const {
    return getActualValue(
        this->state.esp.digitalRead(this->inputPin), this->state.invertInput);
}

bool CoverMovementImpl::isStarted() const {
    return this->startedTime != 0;
}

bool CoverMovementImpl::isReallyMoving() const {
    return this->moveStartPosition != mspNotMoving;
}

int CoverMovementImpl::update() {
    int newPosition = this->state.position;
    auto now = this->state.esp.millis();
    bool moving = this->isMoving();

    if (this->state.latching) {
        if (moving && this->startTriggered) {
            this->log("Reset start");
            this->resetStart();
        }
    }

    const auto paps = this->state.previouslyActivePositionSensor;
    const bool hasActivePositionSensor = this->state.activePositionSensor >= 0;
    if (hasActivePositionSensor) {
        if (paps == noPositionSensor) {
            this->calculateMoveTimeIfNeeded();
        }
        this->moveStartTime = 0;
    } else {
        if (this->state.position != noPosition && this->moveTimeIndex < 0) {
            for (size_t i = 0; i < this->state.positionSensors.size(); ++i) {
                size_t j = this->state.positionSensors.size() - 1 - i;
                if (this->state.position >=
                    this->state.positionSensors[j].position) {
                    if (j < this->state.positionSensors.size() - 1) {
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
                    this->moveStartPosition = this->state.position;
                    this->log("Started moving");
                }

                if (this->state.position == this->endPosition) {
                    newPosition = this->endPosition - this->direction;
                }
            }
        }
    }

    if (this->isReallyMoving()) {
        if (moving) {
            if (!hasActivePositionSensor && this->moveTimeIndex >= 0) {
                const auto& moveTime =
                    this->moveTimes[this->moveTimeIndex].time;
                if (this->state.position != noPosition && moveTime != 0) {
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
        } else if (this->isStarted()) {
            if (!this->state.hasPositionSensors()) {
                this->log("End position reached.");
                newPosition = this->endPosition;
                this->calculateMoveTimeIfNeeded();
            }
            this->handleStopped();
        }
    } else if (
        !moving && this->isStarted() &&
        now - this->startedTime > startTimeout) {
        if (this->state.hasPositionSensors()) {
            this->log("Did not start.");
        } else {
            this->log("Was at end position.");
            newPosition = this->endPosition;
        }
        this->handleStopped();
    }

    if (!moving) {
        if (this->isReallyMoving()) {
            this->log("Stopped moving");
        }

        this->moveStartTime = 0;
        this->moveStartPosition = mspNotMoving;
    }

    return newPosition;
}

void CoverMovementImpl::calculateBeginAndEndPosition() {
    if (this->moveTimeIndex < 0) {
        return;
    }

    if (this->direction > 0) {
        this->beginPosition =
            this->state.positionSensors[this->moveTimeIndex].position;
        this->endPosition =
            this->state.positionSensors[this->moveTimeIndex + 1].position;
    } else {
        this->beginPosition =
            this->state.positionSensors[this->moveTimeIndex + 1].position;
        this->endPosition =
            this->state.positionSensors[this->moveTimeIndex].position;
    }
}

void CoverMovementImpl::calculateMoveTimeIfNeeded() {
    if (this->moveTimeIndex < 0) {
        return;
    }

    auto& moveTime = this->moveTimes[this->moveTimeIndex];
    if (this->moveStartPosition == this->beginPosition) {
        moveTime.time = this->state.esp.millis() - this->moveStartTime;
        this->state.rtc.set(moveTime.rtcId, moveTime.time);
        this->log("Move time: " + tools::intToString(moveTime.time));
    }
}
