#include "TestStream.hpp"

#include <algorithm>
#include <boost/test/unit_test.hpp>

#include "LogExpectation.hpp"

int TestStreambuf::sync() {
    auto s = this->str();
    if (s.back() == '\n') {
        s.resize(s.size() - 1);
    }
    if (this->esp) {
        BOOST_TEST_MESSAGE(this->esp->millis() << " " << s);
    } else {
        BOOST_TEST_MESSAGE(s);
    }
    this->expectations.erase(
        std::remove_if(
            this->expectations.begin(), this->expectations.end(),
            [](const auto& p) { return p.expired(); }),
        this->expectations.end());
    for (auto& exp : this->expectations) {
        exp.lock()->addLog(s);
    }
    this->str("");
    return 0;
}

void TestStreambuf::addExpectation(
    std::shared_ptr<LogExpectation> expectation) {
    this->expectations.push_back(std::move(expectation));
}
