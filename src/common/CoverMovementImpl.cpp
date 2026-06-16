#include "CoverMovementImpl.hpp"

#include "../tools/string.hpp"
#include "CoverHelper.hpp"

namespace {
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
    const auto now = this->state.esp.millis();
    const bool moving = this->isMoving();
    int newPosition = this->state.position;

    this->resetLatchingStartIfMoving(moving);

    // Stop debounce: record when "really moving" first transitioned to
    // "not moving"; clear it on bounce (moving went back to true).
    if (moving) {
        this->stopStartTime = 0;
    } else if (this->isReallyMoving() && this->stopStartTime == 0) {
        this->stopStartTime = now;
    }

    const bool hasActivePositionSensor = this->state.activePositionSensor >= 0;
    if (hasActivePositionSensor) {
        this->updateWithActivePositionSensor();
    } else {
        newPosition =
            this->updateWithoutActivePositionSensor(moving, now, newPosition);
    }

    if (isReallyMoving()) {
        newPosition = this->trackMovement(
            hasActivePositionSensor, moving, now, newPosition);
    }

    if (!this->isReallyMoving() && !moving && this->isStarted() &&
        now - this->startedTime > startTimeout) {
        newPosition = this->checkStartTimeout(newPosition);
    }

    // State reset is also debounced: if we just observed "not moving" and
    // the debounce window hasn't elapsed, the cover is still considered
    // "really moving" (moveStartPosition kept), so the checkStartTimeout
    // branch above does not fire on the next call with stale startedTime.
    if (!moving && this->stopStartTime != 0 &&
        now - this->stopStartTime >= debounceTime) {
        this->resetStateIfStopped();
    }

    return newPosition;
}

void CoverMovementImpl::resetLatchingStartIfMoving(bool moving) {
    if (this->state.latching && moving && this->startTriggered) {
        this->log("Reset start");
        this->resetStart();
    }
}

void CoverMovementImpl::updateWithActivePositionSensor() {
    if (this->state.previouslyActivePositionSensor == noPositionSensor) {
        this->calculateMoveTimeIfNeeded();
    }
    this->moveStartTime = 0;
}

int CoverMovementImpl::updateWithoutActivePositionSensor(
    bool moving, unsigned long now, int newPosition) {
    this->findPositionIndexIfNeeded();
    if (moving) {
        newPosition = this->handleMovingWithoutSensor(now, newPosition);
    }
    return newPosition;
}

void CoverMovementImpl::findPositionIndexIfNeeded() {
    if (this->state.position == noPosition || this->moveTimeIndex >= 0) {
        return;
    }
    for (size_t i = 0; i < this->state.positionSensors.size(); ++i) {
        size_t j = this->state.positionSensors.size() - 1 - i;
        if (this->state.position >= this->state.positionSensors[j].position) {
            if (j < this->state.positionSensors.size() - 1) {
                this->log("Found position index: " + tools::intToString(j));
                this->moveTimeIndex = j;
                this->calculateBeginAndEndPosition();
            }
            break;
        }
    }
}

int CoverMovementImpl::handleMovingWithoutSensor(
    unsigned long now, int newPosition) {
    const auto paps = this->state.previouslyActivePositionSensor;
    if (paps >= 0) {
        return this->handleLeavingSensor(paps, now, newPosition);
    }
    return this->handleDebounceAndEndPosition(now, newPosition);
}

int CoverMovementImpl::handleLeavingSensor(
    int paps, unsigned long now, int newPosition) {
    this->log("Just left position sensor " + tools::intToString(paps));
    this->moveTimeIndex = this->direction > 0 ? paps : paps - 1;
    if (this->moveTimeIndex >= static_cast<int>(this->moveTimes.size())) {
        this->moveTimeIndex = noPositionSensor;
    }
    if (this->moveTimeIndex >= 0) {
        this->moveStartTime = now;
        this->calculateBeginAndEndPosition();
        newPosition = this->beginPosition + this->direction;
        this->moveStartPosition = this->beginPosition;
    }
    return newPosition;
}

int CoverMovementImpl::handleDebounceAndEndPosition(
    unsigned long now, int newPosition) {
    if (this->moveStartTime == 0) {
        this->moveStartTime = now;
    } else if (
        !this->isReallyMoving() && now - this->moveStartTime >= debounceTime) {
        this->moveStartPosition = this->state.position;
        this->log("Started moving");
    }

    if (this->state.position == this->endPosition) {
        newPosition = this->endPosition - this->direction;
    }
    return newPosition;
}

int CoverMovementImpl::trackMovement(
    bool hasActivePositionSensor, bool moving, unsigned long now,
    int newPosition) {
    if (moving) {
        if (!hasActivePositionSensor && this->moveTimeIndex >= 0) {
            return this->interpolatePosition(now, newPosition);
        } else {
            return newPosition;
        }
    }
    if (this->isStarted()) {
        if (this->stopStartTime != 0 &&
            now - this->stopStartTime >= debounceTime) {
            newPosition = this->handleEndOfMovement(newPosition);
        } else if (!hasActivePositionSensor && this->moveTimeIndex >= 0) {
            // Continue interpolating during the stop debounce window,
            // as if the cover were still moving.
            newPosition = this->interpolatePosition(now, newPosition);
        }
    }
    return newPosition;
}

int CoverMovementImpl::interpolatePosition(unsigned long now, int newPosition) {
    const auto& moveTime = this->moveTimes[this->moveTimeIndex].time;
    if (this->state.position == noPosition || moveTime == 0) {
        return this->beginPosition + this->direction;
    }
    const int a =
        (this->endPosition - this->beginPosition) * (now - this->moveStartTime);
    const auto d = static_cast<int>(static_cast<double>(a) / moveTime);
    newPosition = this->moveStartPosition + d;
    if (this->direction * newPosition >= this->endPosition) {
        newPosition = this->endPosition - this->direction;
    }
    return newPosition;
}

int CoverMovementImpl::handleEndOfMovement(int newPosition) {
    if (!this->state.hasPositionSensors()) {
        this->log("End position reached.");
        newPosition = this->endPosition;
        this->calculateMoveTimeIfNeeded();
    }
    this->handleStopped();
    return newPosition;
}

int CoverMovementImpl::checkStartTimeout(int newPosition) {
    if (this->state.hasPositionSensors()) {
        this->log("Did not start.");
    } else {
        this->log("Was at end position.");
        newPosition = this->endPosition;
    }
    this->handleStopped();
    return newPosition;
}

void CoverMovementImpl::resetStateIfStopped() {
    if (this->isReallyMoving()) {
        this->log("Stopped moving");
    }
    this->moveStartTime = 0;
    this->moveStartPosition = mspNotMoving;
    this->stopStartTime = 0;
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
