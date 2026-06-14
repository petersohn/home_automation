#ifndef COVER_UPDATE_HPP
#define COVER_UPDATE_HPP

#include <memory>

#include "Actions.hpp"
#include "CoverMovement.hpp"
#include "CoverState.hpp"
#include "CoverStop.hpp"

class CoverUpdate {
public:
    CoverUpdate(
        CoverState& state, std::unique_ptr<CoverMovement> up,
        std::unique_ptr<CoverMovement> down,
        std::unique_ptr<CoverStop> stopper);

    void update(Actions& action);
    void requestOpen();
    void requestClose();
    void requestStop();
    void requestSetPosition(int value);

private:
    void startDirection(CoverMovement& forward, CoverMovement& reverse);
    void stopAll();
    void updateActivePositionSensor();
    int resolveMovementPosition();
    int updateMovementDirection();
    int applySensorPosition(int newPosition);
    void resetStopperIfStopped();
    void emitStateChange(
        int movementDirection, int newPosition, Actions& action);
    void handleTargetPosition();

    CoverState& state;
    std::unique_ptr<CoverMovement> up;
    std::unique_ptr<CoverMovement> down;
    std::unique_ptr<CoverStop> stopper;
};

#endif  // COVER_UPDATE_HPP
