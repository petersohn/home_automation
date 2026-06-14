#ifndef COVER_MOVEMENT_IMPL_HPP
#define COVER_MOVEMENT_IMPL_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "CoverMovement.hpp"
#include "CoverState.hpp"
#include "CoverStop.hpp"

class CoverMovementImpl : public CoverMovement {
public:
    CoverMovementImpl(
        CoverState& context, CoverStop& stopper, uint8_t inputPin,
        uint8_t outputPin, int endPosition, int direction,
        std::string directionName);

    void start() override;
    void stop() override;
    bool isMoving() const override;
    bool isStarted() const override;
    int update() override;

private:
    struct MoveTime {
        unsigned rtcId;
        unsigned time;
    };

    void resetStarted();
    void resetStart();
    void handleStopped();
    void log(const std::string& msg);
    bool isReallyMoving() const;
    void calculateMoveTimeIfNeeded();
    void calculateBeginAndEndPosition();
    void resetLatchingStartIfMoving(bool moving);
    void updateWithActivePositionSensor();
    int updateWithoutActivePositionSensor(
        bool moving, unsigned long now, int newPosition);
    void findPositionIndexIfNeeded();
    int handleMovingWithoutSensor(unsigned long now, int newPosition);
    int handleLeavingSensor(int paps, unsigned long now, int newPosition);
    int handleDebounceAndEndPosition(unsigned long now, int newPosition);
    int trackMovement(
        bool hasActivePositionSensor, bool moving, unsigned long now,
        int newPosition);
    int interpolatePosition(
        bool hasActivePositionSensor, unsigned long now, int newPosition);
    int handleEndOfMovement(int newPosition);
    int checkStartTimeout(bool moving, unsigned long now, int newPosition);
    void resetStateIfStopped(bool moving);

    CoverState& state;
    CoverStop& stopper;
    const uint8_t inputPin;
    const uint8_t outputPin;
    int beginPosition;
    int endPosition;
    const int direction;
    const std::string debugPrefix;
    std::vector<MoveTime> moveTimes;
    int moveTimeIndex = -1;
    unsigned long moveStartTime = 0;
    unsigned long startedTime = 0;
    int moveStartPosition = -2;
    bool startTriggered = false;
};

#endif  // COVER_MOVEMENT_IMPL_HPP
