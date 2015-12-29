#ifndef TEST_DEVICE_HPP
#define TEST_DEVICE_HPP

#include <Arduino.h>

#include <vector>

namespace device {

extern const char* name;

struct Pin {
    const char* const name;
    const int number;
    const bool output;
    bool status = false;
    unsigned long lastSeen = millis();

    Pin(const char* name, int number, bool output) :
        name(name), number(number), output(output) {}
};

extern std::vector<Pin> pins;

}

#endif // TEST_DEVICE_HPP
