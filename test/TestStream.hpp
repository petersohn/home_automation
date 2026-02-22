#ifndef TEST_TESTSTREAM_HPP
#define TEST_TESTSTREAM_HPP

#include <memory>
#include <sstream>

#include "common/EspApi.hpp"

class LogExpectation;

class TestStreambuf : public std::stringbuf {
public:
    EspApi* esp;

    void addExpectation(std::shared_ptr<LogExpectation> expectation);

protected:
    virtual int sync() override;

private:
    std::vector<std::weak_ptr<LogExpectation>> expectations;
};

#endif  // TEST_TESTSTREAM_HPP
