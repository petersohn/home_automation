#ifndef COMMON_MQTTTRANSPORTSTATE_HPP
#define COMMON_MQTTTRANSPORTSTATE_HPP

#include <cstdint>

struct MqttTransportState {
    bool connected;
    uint32_t generation;
};

#endif  // COMMON_MQTTTRANSPORTSTATE_HPP
