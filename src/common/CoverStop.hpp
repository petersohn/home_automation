#ifndef COVER_STOP_HPP
#define COVER_STOP_HPP

class CoverStop {
public:
    virtual ~CoverStop() = default;
    virtual void stop() = 0;
    virtual void reset() = 0;
    virtual bool isTriggered() const = 0;
    virtual bool isLatching() const = 0;
};

#endif  // COVER_STOP_HPP
