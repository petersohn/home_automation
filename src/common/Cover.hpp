#ifndef COVER_HPP
#define COVER_HPP

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "CoverConfig.hpp"
#include "CoverState.hpp"
#include "CoverUpdate.hpp"
#include "EspApi.hpp"
#include "Interface.hpp"
#include "rtc.hpp"

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
    Cover(
        std::ostream& debug, EspApi& esp, Rtc& rtc, const CoverConfig& config);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    void log(const std::string& msg);

    static CoverUpdate makeUpdateImpl(
        CoverState& state, EspApi& esp, uint8_t upMovementPin,
        uint8_t downMovementPin, uint8_t upPin, uint8_t downPin,
        uint8_t stopPin, bool invertOutput);

    CoverState state;
    CoverUpdate updateImpl;
};

#endif  // COVER_HPP
