#ifndef COVER_MOVEMENT_IMPL_HPP
#define COVER_MOVEMENT_IMPL_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "CoverMovement.hpp"
#include "CoverState.hpp"
#include "CoverStop.hpp"

/**
 * Drives a cover in a single direction (open or close) and reports its
 * position.
 *
 * An output pin is activated to make the cover move, and an input pin is
 * read to detect motion. Position is reported as a percentage from the
 * begin position (0%) to the end position (100%).
 *
 * Position tracking uses a combination of fixed reference points and
 * interpolation:
 * - If position sensors are configured, each sensor is a fixed reference
 *   point. When the cover leaves one sensor and before it reaches the next,
 *   the position is interpolated using the measured travel time between
 *   those two sensors.
 * - If no position sensors are configured, the end position is the only
 *   fixed reference point that is assumed to have been reached when the cover
 *   stops moving.
 *
 * Travel times between adjacent reference points are measured on the fly
 * and persisted in RTC memory, so position interpolation becomes accurate
 * (calibrated) after the first traversal of each segment.
 *
 * Movement is terminated automatically when the end position is reached,
 * when motion stops, or when a start command fails to produce motion
 * within a short timeout (in which case the cover is assumed to have
 * already been at the end position).
 *
 * Edge cases in reported position:
 * - Travel time not yet known (first traversal of a segment, or no prior
 *   calibration in RTC): reports one step past the begin position of the
 *   current segment instead of an interpolated value.
 * - The actual position is unknown and no segment is currently being
 *   traversed: position is reported as unknown (noPosition) and the system
 *   waits for the cover to cross a sensor.
 * - Start timeout with no position sensors: position is reported as the
 *   end position, on the assumption that the cover was already there.
 * - Start timeout with position sensors: the last known position is kept
 *   and a failure to start is logged.
 */
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
    bool isStopDebounceElapsed(unsigned long now) const;
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
    int interpolatePosition(unsigned long now, int newPosition);
    int handleEndOfMovement(int newPosition);
    int checkStartTimeout(int newPosition);
    void resetStateIfStopped();

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
    unsigned long stopStartTime = 0;
    int moveStartPosition = -2;
    bool startTriggered = false;
};

#endif  // COVER_MOVEMENT_IMPL_HPP
