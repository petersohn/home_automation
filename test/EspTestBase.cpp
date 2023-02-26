#include "EspTestBase.hpp"

EspTestBase::EspTestBase() : debug(&debugStreambuf) {
    debugStreambuf.esp = &esp;
}

EspTestBase::~EspTestBase() {
    debugStreambuf.esp = nullptr;
}

void EspTestBase::delayUntil(unsigned long time, unsigned long delay,
        std::function<void()> func) {
    while (esp.millis() < time) {
        esp.delay(std::min(delay, time - esp.millis()));
        func();
    }
}
