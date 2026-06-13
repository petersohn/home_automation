#include "Cover.hpp"

#include <algorithm>
#include <cstdint>

#include "../tools/fromString.hpp"
#include "../tools/string.hpp"

namespace {
constexpr int noPosition = -1;

constexpr int upDirection = 1;
constexpr int downDirection = -1;
}  // namespace

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
    , invertInput(invertInput)
    , invertOutput(invertOutput)
    , invertPositionSensors(invertPositionSensors)
    , closedPosition(closedPosition)
    , positionId(this->rtc.next())
    , context{
          this->position,
          this->stateChanged,
          this->activePositionSensor,
          this->previouslyActivePositionSensor,
          this->previousMovementDirection,
          this->targetPosition,
          this->restartCount,
          this->positionSensors,
          this->invertInput,
          this->invertOutput,
          this->invertPositionSensors,
          this->closedPosition,
          this->positionId,
          this->esp,
          this->rtc,
          this->debug,
          this->debugPrefix,
      }
    , stopper(esp, stopPin, latching, invertOutput, debug, debugPrefix)
    , up(
          context, stopper, upMovementPin, upPin, 100, upDirection,
          std::string("up"))
    , down(
          context, stopper, downMovementPin, downPin, 0, downDirection,
          std::string("down"))
    , updateImpl(context, up, down, stopper) {
    if (this->positionSensors.size() == 1) {
        this->debug
            << "Invalid position sensors: there should be zero or at least 2."
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
        this->debug
            << "Invalid position sensors: positions should go from 0 to 100."
            << std::endl;
        this->positionSensors.clear();
    }

    this->position = this->rtc.get(this->positionId) - 1;
    this->log("Initial position: " + tools::intToString(this->position));
    this->stop();
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
        this->restartCount = 0;
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

void Cover::beginMoving(CoverMovement& direction, CoverMovement& reverse) {
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
    this->updateImpl.update(action);
}

void Cover::stop() {
    this->up.stop();
    this->down.stop();
    this->stopper.stop();
}

void Cover::log(const std::string& msg) {
    this->debug << this->debugPrefix << msg << std::endl;
}
