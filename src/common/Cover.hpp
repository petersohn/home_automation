#ifndef COVER_HPP
#define COVER_HPP

#include <ostream>

#include "EspApi.hpp"
#include "Interface.hpp"
#include "rtc.hpp"

class Cover : public Interface {
public:
    Cover(
        std::ostream& debug, EspApi& esp, Rtc& rtc, uint8_t upMovementPin,
        uint8_t downMovementPin, uint8_t upPin, uint8_t downPin,
        uint8_t stopPin, bool invertInput, bool invertOutput,
        int closedPosition);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
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
        unsigned getDidNotStartCount() const { return didNotStartCount; }
        void resetDidNotStartCount() { didNotStartCount = 0; }
        bool shouldResetStop() const;
        void resetStop();
        void resetStarted();

    private:
        Cover& parent;
        const uint8_t inputPin;
        const uint8_t outputPin;
        const int endPosition;
        const int direction;
        const int timeId;
        const std::string debugPrefix;
        unsigned moveTime = 0;
        unsigned long moveStartTime = 0;
        unsigned long startedTime = 0;
        int moveStartPosition = -2;
        unsigned didNotStartCount = 0;
        bool startTriggered = false;
        bool stopTriggered = false;

        bool isReallyMoving() const;
        void log(const std::string& msg);
        void resetStart();
        void handleStopped();
    };

    std::ostream& debug;
    EspApi& esp;
    Rtc& rtc;

    const std::string debugPrefix;
    Movement up;
    Movement down;
    uint8_t stopPin;
    const bool invertInput;
    const bool invertOutput;
    const int closedPosition;
    const unsigned positionId;

    int position = -1;
    int targetPosition = -1;
    bool stateChanged = false;

    bool isLatching() const;
    bool isMovingUp() const;
    bool isMovingDown() const;
    void stop();
    void resetStop();
    void setOutput(uint8_t pin, bool value);

    void log(const std::string& msg);
    void beginOpening();
    void beginClosing();
    void beginMoving(Movement& direction, Movement& reverse);
    void setPosition(int value);
};

#endif  // COVER_HPP
