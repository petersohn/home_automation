#include "LogExpectation.hpp"

#include <boost/test/unit_test.hpp>

void LogExpectation::addLog(const std::string& log) {
    if (log.find(expectedLog) != std::string::npos) {
        ++count;
    }
}

LogExpectation::~LogExpectation() {
    BOOST_TEST(count == expectedCount);
}
