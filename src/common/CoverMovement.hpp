#ifndef COVER_MOVEMENT_HPP
#define COVER_MOVEMENT_HPP

class CoverMovement {
public:
    virtual ~CoverMovement() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isMoving() const = 0;
    virtual bool isStarted() const = 0;
    virtual int update() = 0;
};

#endif  // COVER_MOVEMENT_HPP
