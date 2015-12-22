#ifndef TEST_DEVICE_HPP
#define TEST_DEVICE_HPP

#include <vector>

namespace device {

extern const char* name;

struct Pin {
    const char* name;
    int number;
    bool output;
};

extern const std::vector<Pin> pins;

}

#endif // TEST_DEVICE_HPP
