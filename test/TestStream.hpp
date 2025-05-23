#ifndef TEST_TESTSTREAM_HPP
#define TEST_TESTSTREAM_HPP

#include <sstream>

#include "common/EspApi.hpp"

class TestStreambuf : public std::stringbuf {
public:
    EspApi* esp;

protected:
    virtual int sync() override;
};

#endif  // TEST_TESTSTREAM_HPP
