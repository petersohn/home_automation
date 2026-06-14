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
    : context(state)
    , up(std::move(up))
    , down(std::move(down))
    , stopper(std::move(stopper)) {}

void CoverUpdate::update(Actions& action) {
    int newPositionSensor = noPositionSensor;
    for (size_t i = 0; i < this->context.positionSensors.size(); ++i) {
        if (getActualValue(
                getActualValue(
                    this->context.esp.digitalRead(
                        this->context.positionSensors[i].pin) != 0,
                    this->context.positionSensors[i].invert),
                this->context.invertPositionSensors)) {
            newPositionSensor = i;
            break;
        }
    }

    if (newPositionSensor != this->context.activePositionSensor) {
        this->context.previouslyActivePositionSensor =
            this->context.activePositionSensor;
        if (newPositionSensor >= 0) {
            this->log(
                "Position sensor activated: " +
                tools::intToString(
                    this->context.positionSensors[newPositionSensor].position));
        } else {
            this->log("Position sensor deactivated");
        }
        this->context.activePositionSensor = newPositionSensor;
    } else {
        this->context.previouslyActivePositionSensor = papsNoChange;
    }

    int newPositionUp = this->up->update();
    int newPositionDown = this->down->update();
    int newPosition = this->context.position;
    if (newPositionUp != this->context.position &&
        newPositionDown != this->context.position) {
        this->log("Inconsistent moving state.");
        newPosition = noPosition;
        this->up->stop();
        this->down->stop();
        this->stopper->stop();
    } else if (newPositionUp != this->context.position) {
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

    if (this->context.previousMovementDirection != movementDirection) {
        this->context.previousMovementDirection = movementDirection;
        this->context.stateChanged = true;
    }

    if (this->context.activePositionSensor != noPositionSensor) {
        newPosition =
            this->context.positionSensors[this->context.activePositionSensor]
                .position;
    }

    if (this->stopper->isTriggered() && !this->up->isMoving() &&
        !this->down->isMoving()) {
        this->stopper->reset();
    }

    if (newPosition != this->context.position || this->context.stateChanged) {
        this->context.position = newPosition;
        this->context.rtc.set(
            this->context.positionId, this->context.position + 1);
        std::string stateName;

        if (movementDirection == upDirection) {
            stateName = "OPENING";
        } else if (movementDirection == downDirection) {
            stateName = "CLOSING";
        } else if (this->context.position <= this->context.closedPosition) {
            stateName = "CLOSED";
        } else {
            stateName = "OPEN";
        }

        this->log(
            "state=" + stateName +
            " position=" + tools::intToString(this->context.position));

        std::vector<std::string> values{std::move(stateName)};
        if (this->context.position != noPosition) {
            values.push_back(tools::intToString(this->context.position));
        }
        action.fire(values);

        this->context.stateChanged = false;
    }

    if (this->context.targetPosition != noPosition) {
        enum class Action { Nothing, Restart, Reset };
        Action restartAction = Action::Nothing;

        if (this->context.position == this->context.targetPosition) {
            restartAction = Action::Reset;
        } else if (!this->up->isStarted() && !this->down->isStarted()) {
            if (this->context.hasPositionSensors() &&
                this->context.position != 0 && this->context.position != 100) {
                restartAction = Action::Reset;
            } else if (this->context.restartCount < 3) {
                restartAction = Action::Restart;
            } else {
                restartAction = Action::Reset;
            }
        }

        switch (restartAction) {
        case Action::Restart:
            ++this->context.restartCount;
            if (this->context.targetPosition < this->context.position) {
                this->up->stop();
                this->down->start();
            } else {
                this->down->stop();
                this->up->start();
            }
            this->context.stateChanged = true;
            break;
        case Action::Reset:
            this->context.targetPosition = noPosition;
            this->context.restartCount = 0;
            this->up->stop();
            this->down->stop();
            this->stopper->stop();
            break;
        case Action::Nothing:
            break;
        }
    }
}

void CoverUpdate::requestOpen() {
    this->context.targetPosition = -1;
    if (!this->up->isStarted()) {
        this->down->stop();
        this->up->start();
        this->context.stateChanged = true;
    }
}

void CoverUpdate::requestStop() {
    this->context.targetPosition = -1;
    this->up->stop();
    this->down->stop();
    this->stopper->stop();
}

void CoverUpdate::requestClose() {
    this->context.targetPosition = -1;
    if (!this->down->isStarted()) {
        this->up->stop();
        this->down->start();
        this->context.stateChanged = true;
    }
}

void CoverUpdate::requestSetPosition(int value) {
    if (value < 0 || value > 100) {
        this->log("Position out of range: " + tools::intToString(value));
        return;
    }

    if (this->context.position == -1) {
        this->log("Position is not known, calibrating.");
    }

    this->context.targetPosition = value;
    this->context.restartCount = 0;

    if (value < this->context.position) {
        if (!this->down->isStarted()) {
            this->up->stop();
            this->down->start();
            this->context.stateChanged = true;
        }
    } else if (value > this->context.position) {
        if (!this->up->isStarted()) {
            this->down->stop();
            this->up->start();
            this->context.stateChanged = true;
        }
    } else {
        this->up->stop();
        this->down->stop();
        this->stopper->stop();
    }
}

void CoverUpdate::log(const std::string& msg) {
    this->context.debug << this->context.debugPrefix << msg << std::endl;
}
