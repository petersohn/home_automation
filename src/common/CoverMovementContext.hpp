#ifndef COVER_MOVEMENT_CONTEXT_HPP
#define COVER_MOVEMENT_CONTEXT_HPP

#include <ostream>
#include <string>
#include <vector>

#include "EspApi.hpp"
#include "PositionSensor.hpp"
#include "rtc.hpp"

struct CoverMovementContext {
    // Mutable state (owned by the context)
    int position = -1;
    bool stateChanged = false;
    int activePositionSensor = -1;
    int previouslyActivePositionSensor = -1;
    int previousMovementDirection = 0;
    int targetPosition = -1;
    unsigned restartCount = 0;

    // Immutable config
    std::vector<PositionSensor> positionSensors;
    bool invertInput;
    bool invertOutput;
    bool invertPositionSensors;
    int closedPosition;
    unsigned positionId;

    // Services
    EspApi& esp;
    Rtc& rtc;
    std::ostream& debug;
    std::string debugPrefix;

    bool hasPositionSensors() const { return !this->positionSensors.empty(); }
};

#endif  // COVER_MOVEMENT_CONTEXT_HPP
