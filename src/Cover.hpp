#ifndef COVER_HPP
#define COVER_HPP

#include "common/Interface.hpp"

class Cover : public Interface {
public:
    enum class State {
        Stopping,
        Idle,
        BeginOpening,
        Opening,
        BeginClosing,
        Closing,
    };

    Cover(int movementPin, int upPin, int downPin,
            bool invertMovement, bool invertUpDown);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    const int movementPin;
    const int upPin;
    const int downPin;
    const bool invertMovement;
    const bool invertUpDown;
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

    bool isMoving() const;
    void stop();

    void log(const std::string& msg);
    void beginOpening();
    void beginClosing();
    void setPosition(int value);
};


#endif // COVER_HPP
