#include "Cover.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>

#include "../tools/fromString.hpp"
#include "../tools/string.hpp"
#include "CoverMovementImpl.hpp"
#include "CoverStopImpl.hpp"

Cover::Cover(
    std::ostream& debug, EspApi& esp, Rtc& rtc, uint8_t upMovementPin,
    uint8_t downMovementPin, uint8_t upPin, uint8_t downPin, uint8_t stopPin,
    bool latching, bool invertInput, bool invertOutput, int closedPosition,
    std::vector<PositionSensor> positionSensors, bool invertPositionSensors)
    : state{
          -1,            // position
          false,         // stateChanged
          -1,            // activePositionSensor
          -1,            // previouslyActivePositionSensor
          0,             // previousMovementDirection
          -1,            // targetPosition
          0,             // restartCount
          std::move(positionSensors),
          invertInput,
          invertOutput,
          invertPositionSensors,
          closedPosition,
          rtc.next(),    // positionId
          esp,           // esp
          rtc,           // rtc
          debug,         // debug
          "Cover " + tools::intToString(upPin) + "." +
              tools::intToString(downPin) + ": ",  // debugPrefix
      },
      updateImpl(makeUpdateImpl(
          state, esp, upMovementPin, downMovementPin, upPin, downPin, stopPin,
          latching, invertOutput, debug)) {
    if (this->state.positionSensors.size() == 1) {
        this->state.debug
            << "Invalid position sensors: there should be zero or at least 2."
            << std::endl;
        this->state.positionSensors.clear();
    }

    std::sort(
        this->state.positionSensors.begin(),
        this->state.positionSensors.end(),
        [](const PositionSensor& lhs, const PositionSensor& rhs) {
            return lhs.position < rhs.position;
        });

    if (!this->state.positionSensors.empty() &&
        (this->state.positionSensors.front().position != 0 ||
         this->state.positionSensors.back().position != 100)) {
        this->state.debug
            << "Invalid position sensors: positions should go from 0 to 100."
            << std::endl;
        this->state.positionSensors.clear();
    }

    this->state.position = this->state.rtc.get(this->state.positionId) - 1;
    this->log(
        "Initial position: " + tools::intToString(this->state.position));
}

CoverUpdate Cover::makeUpdateImpl(
    CoverState& state, EspApi& esp, uint8_t upMovementPin,
    uint8_t downMovementPin, uint8_t upPin, uint8_t downPin,
    uint8_t stopPin, bool latching, bool invertOutput,
    std::ostream& debug) {
    auto stopper = std::make_unique<CoverStopImpl>(
        esp, stopPin, latching, invertOutput, debug, state.debugPrefix);
    auto up = std::make_unique<CoverMovementImpl>(
        state, *stopper, upMovementPin, upPin, 100, 1, "up");
    auto down = std::make_unique<CoverMovementImpl>(
        state, *stopper, downMovementPin, downPin, 0, -1, "down");
    return CoverUpdate(
        state, std::move(up), std::move(down), std::move(stopper));
}

void Cover::start() {
    this->state.stateChanged = true;
}

void Cover::execute(const std::string& command) {
    if (command == "STOP") {
        this->updateImpl.requestStop();
    } else if (command == "OPEN") {
        this->updateImpl.requestOpen();
    } else if (command == "CLOSE") {
        this->updateImpl.requestClose();
    } else {
        auto pos = tools::fromString<int>(command);
        if (!pos.has_value()) {
            this->log("Invalid command: " + command);
            return;
        }
        this->updateImpl.requestSetPosition(*pos);
    }
}

void Cover::update(Actions action) {
    this->updateImpl.update(action);
}

void Cover::log(const std::string& msg) {
    this->state.debug << this->state.debugPrefix << msg << std::endl;
}
