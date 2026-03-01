#ifndef COMMON_LOCK_HPP
#define COMMON_LOCK_HPP

#include <cstdint>

class Lock {
public:
    void lock() { ++this->lockCount; }

    void unlock() {
        if (this->lockCount != 0) {
            --this->lockCount;
        }
    }

    bool isFree() const { return this->lockCount == 0; }

private:
    uint32_t lockCount = 0;
};

#endif  // COMMON_LOCK_HPP
