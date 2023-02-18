#ifndef TEST_TESTSTREAM_HPP
#define TEST_TESTSTREAM_HPP

#include <sstream>

class TestStreambuf: public std::stringbuf {
protected:
    virtual int sync() override;
};

#endif // TEST_TESTSTREAM_HPP

