#include "CoverUpdate.hpp"

#include "../tools/string.hpp"

namespace {
bool getActualValue(bool value, bool invert) {
    return invert ? !value : value;
}

constexpr int papsNoChange = -2;
constexpr int noPosition = -1;
constexpr int noPositionSensor = -1;

constexpr int upDirection = 1;
constexpr int downDirection = -1;
}  // namespace

CoverUpdate::CoverUpdate(
    CoverState& state, std::unique_ptr<CoverMovement> up,
    std::unique_ptr<CoverMovement> down, std::unique_ptr<CoverStop> stopper)
    : state(state)
    , up(std::move(up))
    , down(std::move(down))
    , stopper(std::move(stopper)) {}

void CoverUpdate::update(Actions& action) {
    int newPositionSensor = noPositionSensor;
    for (size_t i = 0; i < this->state.positionSensors.size(); ++i) {
        if (getActualValue(
                getActualValue(
                    this->state.esp.digitalRead(
                        this->state.positionSensors[i].pin) != 0,
                    this->state.positionSensors[i].invert),
                this->state.invertPositionSensors)) {
            newPositionSensor = i;
            break;
        }
    }

    if (newPositionSensor != this->state.activePositionSensor) {
        this->state.previouslyActivePositionSensor =
            this->state.activePositionSensor;
        if (newPositionSensor >= 0) {
            this->state.log(
                "Position sensor activated: " +
                tools::intToString(
                    this->state.positionSensors[newPositionSensor].position));
        } else {
            this->state.log("Position sensor deactivated");
        }
        this->state.activePositionSensor = newPositionSensor;
    } else {
        this->state.previouslyActivePositionSensor = papsNoChange;
    }

    int newPositionUp = this->up->update();
    int newPositionDown = this->down->update();
    int newPosition = this->state.position;
    if (newPositionUp != this->state.position &&
        newPositionDown != this->state.position) {
        this->state.log("Inconsistent moving state.");
        newPosition = noPosition;
        this->stopAll();
    } else if (newPositionUp != this->state.position) {
        newPosition = newPositionUp;
    } else {
        newPosition = newPositionDown;
    }

    int movementDirection = 0;
    if (this->up->isMoving()) {
        movementDirection = upDirection;
    } else if (this->down->isMoving()) {
        movementDirection = downDirection;
    }

    if (this->state.previousMovementDirection != movementDirection) {
        this->state.previousMovementDirection = movementDirection;
        this->state.stateChanged = true;
    }

    if (this->state.activePositionSensor != noPositionSensor) {
        newPosition =
            this->state.positionSensors[this->state.activePositionSensor]
                .position;
    }

    if (this->stopper->isTriggered() && !this->up->isMoving() &&
        !this->down->isMoving()) {
        this->stopper->reset();
    }

    if (newPosition != this->state.position || this->state.stateChanged) {
        this->state.position = newPosition;
        this->state.rtc.set(
            this->state.positionId, this->state.position + 1);
        std::string stateName;

        if (movementDirection == upDirection) {
            stateName = "OPENING";
        } else if (movementDirection == downDirection) {
            stateName = "CLOSING";
        } else if (this->state.position <= this->state.closedPosition) {
            stateName = "CLOSED";
        } else {
            stateName = "OPEN";
        }

        this->state.log(
            "state=" + stateName +
            " position=" + tools::intToString(this->state.position));

        std::vector<std::string> values{std::move(stateName)};
        if (this->state.position != noPosition) {
            values.push_back(tools::intToString(this->state.position));
        }
        action.fire(values);

        this->state.stateChanged = false;
    }

    if (this->state.targetPosition != noPosition) {
        enum class Action { Nothing, Restart, Reset };
        Action restartAction = Action::Nothing;

        if (this->state.position == this->state.targetPosition) {
            restartAction = Action::Reset;
        } else if (!this->up->isStarted() && !this->down->isStarted()) {
            if (this->state.hasPositionSensors() &&
                this->state.position != 0 && this->state.position != 100) {
                restartAction = Action::Reset;
            } else if (this->state.restartCount < 3) {
                restartAction = Action::Restart;
            } else {
                restartAction = Action::Reset;
            }
        }

        switch (restartAction) {
        case Action::Restart:
            ++this->state.restartCount;
            if (this->state.targetPosition < this->state.position) {
                this->startDirection(*this->down, *this->up);
            } else {
                this->startDirection(*this->up, *this->down);
            }
            break;
        case Action::Reset:
            this->state.targetPosition = noPosition;
            this->state.restartCount = 0;
            this->stopAll();
            break;
        case Action::Nothing:
            break;
        }
    }
}

void CoverUpdate::requestOpen() {
    this->state.targetPosition = -1;
    this->startDirection(*this->up, *this->down);
}

void CoverUpdate::requestStop() {
    this->state.targetPosition = -1;
    this->stopAll();
}

void CoverUpdate::requestClose() {
    this->state.targetPosition = -1;
    this->startDirection(*this->down, *this->up);
}

void CoverUpdate::requestSetPosition(int value) {
    if (value < 0 || value > 100) {
        this->state.log("Position out of range: " + tools::intToString(value));
        return;
    }

    if (this->state.position == -1) {
        this->state.log("Position is not known, calibrating.");
    }

    this->state.targetPosition = value;
    this->state.restartCount = 0;

    if (value < this->state.position) {
        this->startDirection(*this->down, *this->up);
    } else if (value > this->state.position) {
        this->startDirection(*this->up, *this->down);
    } else {
        this->stopAll();
    }
}

void CoverUpdate::startDirection(CoverMovement& forward, CoverMovement& reverse) {
    if (!forward.isStarted()) {
        reverse.stop();
        forward.start();
        this->state.stateChanged = true;
    }
}

void CoverUpdate::stopAll() {
    this->up->stop();
    this->down->stop();
    this->stopper->stop();
}
