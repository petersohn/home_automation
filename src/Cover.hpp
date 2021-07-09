#ifndef COVER_HPP
#define COVER_HPP

#include "common/Interface.hpp"

class Cover : public Interface {
public:
    Cover(int upMovementPin, int downMovementPin, int upPin, int downPin,
            bool invertInput, bool invertOutput);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    class Movement {
    public:
        Movement(Cover& parent, int inputPin, int outputPin, int endPosition,
                int direction, const std::string& directionName);
        int update();
        void start();
        void stop();
        bool isMoving() const;
        bool isStarted() const;
        unsigned getDidNotStartCount() const { return didNotStartCount; }
        void resetDidNotStartCount() { didNotStartCount = 0; }

    private:
        Cover& parent;
        const int inputPin;
        const int outputPin;
        const int endPosition;
        const int direction;
        const int timeId;
        const std::string debugPrefix;
        unsigned moveTime = 0;
        unsigned long moveStartTime = 0;
        unsigned long startedTime = 0;
        int moveStartPosition = -2;
        unsigned didNotStartCount = 0;

        bool isReallyMoving() const;
        void log(const std::string& msg);
    };

    const std::string debugPrefix;
    Movement up;
    Movement down;
    const bool invertInput;
    const bool invertOutput;
    const unsigned positionId;

    int position = -1;
    int targetPosition = -1;
    bool stateChanged = false;

    bool isMovingUp() const;
    bool isMovingDown() const;
    void stop();

    void log(const std::string& msg);
    void beginOpening();
    void beginClosing();
    void setPosition(int value);
};


#endif // COVER_HPP
