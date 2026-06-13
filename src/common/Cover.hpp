#ifndef COVER_HPP
#define COVER_HPP

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "EspApi.hpp"
#include "Interface.hpp"
#include "PositionSensor.hpp"
#include "rtc.hpp"

class CoverMovement;

// Full includes for direct member instances
#include "CoverMovementContext.hpp"
#include "CoverMovementImpl.hpp"
#include "CoverStop.hpp"
#include "CoverUpdate.hpp"

/**
 * Controls a cover (gate, window shutter, etc.).
 *
 * Output:
 * 1. state: OPENING, CLOSING, OPEN, CLOSED
 * 2. position
 *
 * Position calculation: The time between known positions is measured, and once
 * it is known, the position is interpolated by the time the cover moves between
 * known positions.
 * - If there are position sensors, then these are used as fixed points.
 * - If there are no position sensors, then the cover stopping is
 *   interpreted as reaching the end position, which is then used as a fixed
 *   point.
 *
 * Calibration: if the opening/closing time is not known when an exact position
 * command is received, open and close until the timing is calibrated, then set
 * the position.
 *
 * Input commands:
 * - OPEN: start opening. No calibration.
 * - CLOSE: start closing. No calibration.
 * - STOP: stop moving.
 * - <number>: Set to a target position. Calibrate if needed.
 *
 * Output:
 * - In latching mode, the up or down pin is activated, then once it begins
 *   moving, the pin is released. It is stopped by activating the stop pin.
 * - In continuous mode, the up or down pin is held active while the cover
 *   should be moving.
 *
 * Positive/negative logic: Invert parameters decide how inputs and outputs are
 * treated. If inversion is true, outputs work in negative logic (1=false,
 * 0=true). If it is false, outputs work in posirtive logic (0=false, 1=true).
 */
class Cover : public Interface {
public:
    /**
     * @param upMovementPin Input for detecting that the cover is opening.
     * @param downMovementPin Input for detecting that the cover is closing.
     * @param upPin Output for controlling opening movement.
     * @param downPin Output for controlling closing movement.
     * @param stopPin Output for stopping movement. Only used if latching is
     * true.
     * @param latching If true, the device works in latching mode and uses
     * stopPin. If false, the device works in continuous mode and stopPin is
     * ignored.
     * @param invertInput Controls inversion of upMovementPin and
     * downMovementPin.
     * @param invertOutput Controls inversion of upPin and downPin.
     * @param closedPosition Controls the reported state when the cover is not
     * moving. If the position is above this value, report as open. Otherwise,
     * report as closed.
     * @param invertPositionSensors Controls inversion of position sensors. This
     * inverts all position sensors, while each position sensor can be inverted
     * individually. The two are cumulative: if this parameter is true and the
     * position sensor is inverted, then that sensor will use positive logic.
     */
    Cover(
        std::ostream& debug, EspApi& esp, Rtc& rtc, uint8_t upMovementPin,
        uint8_t downMovementPin, uint8_t upPin, uint8_t downPin,
        uint8_t stopPin, bool latching, bool invertInput, bool invertOutput,
        int closedPosition, std::vector<PositionSensor> positionSensors,
        bool invertPositionSensors);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    void stop();
    void log(const std::string& msg);
    void beginOpening();
    void beginClosing();
    void beginMoving(CoverMovement& direction, CoverMovement& reverse);
    void setPosition(int value);

    std::ostream& debug;
    EspApi& esp;
    Rtc& rtc;

    const std::string debugPrefix;
    const bool invertOutput;

    CoverMovementContext context;
    CoverStop stopper;
    CoverMovementImpl up;
    CoverMovementImpl down;
    CoverUpdate updateImpl;
};

#endif  // COVER_HPP
