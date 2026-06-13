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
    CoverMovementContext& context, CoverStop& stopper, uint8_t inputPin,
    uint8_t outputPin, int endPosition, int direction,
    std::string directionName)
    : context(context)
    , stopper(stopper)
    , inputPin(inputPin)
    , outputPin(outputPin)
    , beginPosition(100 - endPosition)
    , endPosition(endPosition)
    , direction(direction)
    , debugPrefix(this->context.debugPrefix + directionName + ": ") {
    this->context.esp.pinMode(inputPin, GpioMode::input);
    this->context.esp.pinMode(outputPin, GpioMode::output);
    this->context.esp.digitalWrite(
        this->outputPin, this->context.invertOutput ? 1 : 0);
    size_t timeCount = 1;
    if (this->context.hasPositionSensors()) {
        timeCount = this->context.positionSensors.size() - 1;
    }

    if (timeCount == 1) {
        this->moveTimeIndex = 0;
    }

    this->moveTimes.reserve(timeCount);
    for (size_t i = 0; i < timeCount; ++i) {
        const auto id = this->context.rtc.next();
        this->moveTimes.emplace_back(MoveTime{id, this->context.rtc.get(id)});
    }
}

void CoverMovementImpl::start() {
    this->stopper.reset();
    this->log("Start");
    this->context.esp.digitalWrite(
        this->outputPin, this->context.invertOutput ? 0 : 1);
    this->startTriggered = true;
    if (!this->isStarted()) {
        this->startedTime = this->context.esp.millis();
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
        this->context.stateChanged = true;
    }
}

void CoverMovementImpl::resetStart() {
    this->context.esp.digitalWrite(
        this->outputPin, this->context.invertOutput ? 1 : 0);
    this->startTriggered = false;
}

void CoverMovementImpl::handleStopped() {
    if (!this->stopper.isLatching() || this->startTriggered) {
        this->stop();
    } else {
        this->resetStarted();
    }
}

void CoverMovementImpl::log(const std::string& msg) {
    this->context.debug << this->debugPrefix << msg << std::endl;
}

bool CoverMovementImpl::isMoving() const {
    return getActualValue(
        this->context.esp.digitalRead(this->inputPin),
        this->context.invertInput);
}

bool CoverMovementImpl::isStarted() const {
    return this->startedTime != 0;
}

bool CoverMovementImpl::isReallyMoving() const {
    return this->moveStartPosition != mspNotMoving;
}

int CoverMovementImpl::update() {
    int newPosition = this->context.position;
    auto now = this->context.esp.millis();
    bool moving = this->isMoving();

    if (this->stopper.isLatching()) {
        if (moving && this->startTriggered) {
            this->log("Reset start");
            this->resetStart();
        }
    }

    const auto paps = this->context.previouslyActivePositionSensor;
    const bool hasActivePositionSensor =
        this->context.activePositionSensor >= 0;
    if (hasActivePositionSensor) {
        if (paps == noPositionSensor) {
            this->calculateMoveTimeIfNeeded();
        }
        this->moveStartTime = 0;
    } else {
        if (this->context.position != noPosition && this->moveTimeIndex < 0) {
            for (size_t i = 0; i < this->context.positionSensors.size(); ++i) {
                size_t j = this->context.positionSensors.size() - 1 - i;
                if (this->context.position >=
                    this->context.positionSensors[j].position) {
                    if (j < this->context.positionSensors.size() - 1) {
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
                    this->moveStartPosition = this->context.position;
                    this->log("Started moving");
                }

                if (this->context.position == this->endPosition) {
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
                if (this->context.position != noPosition && moveTime != 0) {
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
            if (!this->context.hasPositionSensors()) {
                this->log("End position reached.");
                newPosition = this->endPosition;
                this->calculateMoveTimeIfNeeded();
            }
            this->handleStopped();
        }
    } else if (
        !moving && this->isStarted() &&
        now - this->startedTime > startTimeout) {
        if (this->context.hasPositionSensors()) {
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
            this->context.positionSensors[this->moveTimeIndex].position;
        this->endPosition =
            this->context.positionSensors[this->moveTimeIndex + 1].position;
    } else {
        this->beginPosition =
            this->context.positionSensors[this->moveTimeIndex + 1].position;
        this->endPosition =
            this->context.positionSensors[this->moveTimeIndex].position;
    }
}

void CoverMovementImpl::calculateMoveTimeIfNeeded() {
    if (this->moveTimeIndex < 0) {
        return;
    }

    auto& moveTime = this->moveTimes[this->moveTimeIndex];
    if (this->moveStartPosition == this->beginPosition) {
        moveTime.time = this->context.esp.millis() - this->moveStartTime;
        this->context.rtc.set(moveTime.rtcId, moveTime.time);
        this->log("Move time: " + tools::intToString(moveTime.time));
    }
}
