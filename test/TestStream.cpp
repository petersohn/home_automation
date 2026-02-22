#include "TestStream.hpp"

#include <algorithm>
#include <boost/test/unit_test.hpp>

#include "LogExpectation.hpp"

int TestStreambuf::sync() {
    auto s = this->str();
    if (s.back() == '\n') {
        s.resize(s.size() - 1);
    }
    if (esp) {
        BOOST_TEST_MESSAGE(esp->millis() << " " << s);
    } else {
        BOOST_TEST_MESSAGE(s);
    }
    expectations.erase(
        std::remove_if(
            expectations.begin(), expectations.end(),
            [](const auto& p) { return p.expired(); }),
        expectations.end());
    for (auto& exp : expectations) {
        exp.lock()->addLog(s);
    }
    this->str("");
    return 0;
}

void TestStreambuf::addExpectation(
    std::shared_ptr<LogExpectation> expectation) {
    expectations.push_back(std::move(expectation));
}
