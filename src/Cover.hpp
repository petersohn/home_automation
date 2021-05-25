#ifndef COVER_HPP
#define COVER_HPP

#include "common/Interface.hpp"

class Cover : public Interface {
public:
    enum class State {
        Idle,
        Opening,
        Closing,
    };

    Cover(int upMovementPin, int downMovementPin, int upPin, int downPin,
            bool invertInput, bool invertOutput);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    const int upMovementPin;
    const int downMovementPin;
    const int upPin;
    const int downPin;
    const bool invertInput;
    const bool invertOutput;
    const unsigned upTimeId;
    const unsigned downTimeId;
    const unsigned positionId;

    State state = State::Idle;
    unsigned upTime = 0;
    unsigned downTime = 0;
    int position = 0;
    int targetPosition = -1;
    int moveStartPosition = -1;
    unsigned long moveStartTime = 0;
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
