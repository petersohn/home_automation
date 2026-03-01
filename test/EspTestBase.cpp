#include "EspTestBase.hpp"

#include "LogExpectation.hpp"

EspTestBase::EspTestBase() : debug(&this->debugStreambuf) {
    this->debugStreambuf.esp = &this->esp;
}

EspTestBase::~EspTestBase() {
    this->debugStreambuf.esp = nullptr;
}

void EspTestBase::delayUntil(
    unsigned long time, unsigned long delay, std::function<void()> func) {
    while (this->esp.millis() < time) {
        this->esp.delay(std::min(delay, time - this->esp.millis()));
        func();
    }
}

std::shared_ptr<LogExpectation> EspTestBase::expectLog(
    std::string log, size_t count) {
    auto expectation = std::make_shared<LogExpectation>(log, count);
    this->debugStreambuf.addExpectation(expectation);
    return expectation;
}
