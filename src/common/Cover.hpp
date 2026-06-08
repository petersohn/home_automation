#ifndef COVER_HPP
#define COVER_HPP

#include <ostream>

#include "EspApi.hpp"
#include "Interface.hpp"
#include "rtc.hpp"

struct PositionSensor {
    int position = 0;
    uint8_t pin = 0;
    bool invert = false;
};

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
    struct MoveTime {
        unsigned rtcId;
        unsigned time;
    };

    class Movement {
    public:
        Movement(
            Cover& parent, uint8_t inputPin, uint8_t outputPin, int endPosition,
            int direction, const std::string& directionName);
        int update();
        void start();
        void stop();
        bool isMoving() const;
        bool isStarted() const;
        unsigned getDidNotStartCount() const { return this->didNotStartCount; }
        void resetDidNotStartCount() { this->didNotStartCount = 0; }
        void resetStarted();

    private:
        Cover& parent;
        const uint8_t inputPin;
        const uint8_t outputPin;
        int beginPosition;
        int endPosition;
        const int direction;
        const std::string debugPrefix;
        std::vector<MoveTime> moveTimes;
        int moveTimeIndex = -1;
        unsigned long moveStartTime = 0;
        unsigned long moveStopTime = 0;
        unsigned long startedTime = 0;
        int moveStartPosition = -2;
        unsigned didNotStartCount = 0;
        bool startTriggered = false;

        bool isReallyMoving() const;
        void log(const std::string& msg);
        void resetStart();
        void handleStopped();
        void calculateMoveTimeIfNeeded();
        void calculateBeginAndEndPosition();
    };

    class Stop {
    public:
        Stop(Cover& parent, uint8_t pin, bool latching);
        void stop();
        void reset();
        bool isTriggered() const;
        bool isLatching() const;

    private:
        Cover& parent;
        const uint8_t pin;
        const bool latching;
        bool triggered = false;
    };

    std::ostream& debug;
    EspApi& esp;
    Rtc& rtc;

    const std::string debugPrefix;
    std::vector<PositionSensor> positionSensors;
    Stop stopper;
    Movement up;
    Movement down;
    const bool invertInput;
    const bool invertOutput;
    const int closedPosition;
    const bool invertPositionSensors;
    const unsigned positionId;

    int position = -1;
    int activePositionSensor = -1;
    int previouslyActivePositionSensor = -1;
    int targetPosition = -1;
    bool stateChanged = false;
    int previousMovementDirection = 0;

    bool hasPositionSensors() const;
    bool isMovingUp() const;
    bool isMovingDown() const;
    void stop();
    void setOutput(uint8_t pin, bool value);

    void log(const std::string& msg);
    void beginOpening();
    void beginClosing();
    void beginMoving(Movement& direction, Movement& reverse);
    void setPosition(int value);
};

#endif  // COVER_HPP
