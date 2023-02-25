#ifndef TEST_DUMMYBACKOFF_HPP
#define TEST_DUMMYBACKOFF_HPP

#include "common/Backoff.hpp"

class DummyBackoff : public Backoff{
public:
    virtual void good() override {}
    virtual void bad() override {}
};

#endif // TEST_DUMMYBACKOFF_HPP
