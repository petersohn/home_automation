#ifndef COMMON_BACKOFF_HPP
#define COMMON_BACKOFF_HPP

class Backoff {
public:
    virtual void good() = 0;
    virtual void bad() = 0;

    virtual ~Backoff() {}
};

#endif  // COMMON_BACKOFF_HPP
