#include "LogExpectation.hpp"

#include <boost/test/unit_test.hpp>

void LogExpectation::addLog(const std::string& log) {
    if (log.find(this->expectedLog) != std::string::npos) {
        ++this->count;
    }
}

LogExpectation::~LogExpectation() {
    BOOST_TEST(this->count == this->expectedCount);
}
