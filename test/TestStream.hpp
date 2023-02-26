#ifndef TEST_TESTSTREAM_HPP
#define TEST_TESTSTREAM_HPP

#include "common/EspApi.hpp"

#include <sstream>

class TestStreambuf: public std::stringbuf {
public:
    EspApi* esp;
protected:
    virtual int sync() override;
};

#endif // TEST_TESTSTREAM_HPP

