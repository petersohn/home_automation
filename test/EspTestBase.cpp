#include "EspTestBase.hpp"

#include "LogExpectation.hpp"

EspTestBase::EspTestBase() : debug(&debugStreambuf) {
    debugStreambuf.esp = &esp;
}

EspTestBase::~EspTestBase() {
    debugStreambuf.esp = nullptr;
}

void EspTestBase::delayUntil(
    unsigned long time, unsigned long delay, std::function<void()> func) {
    while (esp.millis() < time) {
        esp.delay(std::min(delay, time - esp.millis()));
        func();
    }
}

std::shared_ptr<LogExpectation> EspTestBase::expectLog(
    std::string log, size_t count) {
    auto expectation = std::make_shared<LogExpectation>(log, count);
    debugStreambuf.addExpectation(expectation);
    return expectation;
}
