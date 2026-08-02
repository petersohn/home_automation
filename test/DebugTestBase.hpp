#ifndef TEST_DEBUGTESTBASE_HPP
#define TEST_DEBUGTESTBASE_HPP

#include <gtest/gtest.h>

#include <ostream>

#include "TestStream.hpp"

class DebugTestBase : public ::testing::Test {
public:
    TestStreambuf debugStreambuf;
    std::ostream debug;
    DebugTestBase();
};

#endif  // TEST_DEBUGTESTBASE_HPP