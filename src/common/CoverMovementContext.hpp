#ifndef COVER_MOVEMENT_CONTEXT_HPP
#define COVER_MOVEMENT_CONTEXT_HPP

#include <ostream>
#include <string>
#include <vector>

#include "EspApi.hpp"
#include "PositionSensor.hpp"
#include "rtc.hpp"

struct CoverMovementContext {
    // Mutable state (references to fields owned by Cover)
    int& position;
    bool& stateChanged;
    int& activePositionSensor;
    int& previouslyActivePositionSensor;
    int& previousMovementDirection;
    int& targetPosition;
    unsigned& restartCount;

    // Immutable config
    const std::vector<PositionSensor>& positionSensors;
    const bool invertInput;
    const bool invertOutput;
    const bool invertPositionSensors;
    const int closedPosition;
    const unsigned positionId;

    // Services
    EspApi& esp;
    Rtc& rtc;
    std::ostream& debug;
    const std::string& debugPrefix;

    bool hasPositionSensors() const { return !this->positionSensors.empty(); }
};

#endif  // COVER_MOVEMENT_CONTEXT_HPP
