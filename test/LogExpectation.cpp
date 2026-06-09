#include "LogExpectation.hpp"

#include <gtest/gtest.h>

void LogExpectation::addLog(const std::string& log) {
    if (log.find(this->expectedLog) != std::string::npos) {
        ++this->count;
    }
}

LogExpectation::~LogExpectation() {
    EXPECT_TRUE(this->count == this->expectedCount);
}
