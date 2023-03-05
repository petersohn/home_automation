#ifndef COMMON_LOCK_HPP
#define COMMON_LOCK_HPP

#include <cstdint>

class Lock {
public:
    void lock() { ++lockCount; }

    void unlock() {
        if (lockCount != 0) {
            --lockCount;
        }
    }

    bool isFree() const { return lockCount == 0; }

private:
    uint32_t lockCount = 0;
};

#endif // COMMON_LOCK_HPP
