#ifndef COMMON_MQTT_DRAIN_HPP
#define COMMON_MQTT_DRAIN_HPP

#include <cstdint>

inline bool isTcpDrainComplete(
    bool established, int availableForWrite, int sendBufferCapacity) {
    return established && availableForWrite >= sendBufferCapacity;
}

inline bool isDrainDeadlineExpired(
    uint32_t startedAtMs, uint32_t nowMs, uint32_t timeoutMs) {
    return static_cast<uint32_t>(nowMs - startedAtMs) >= timeoutMs;
}

template <typename IsConnected, typename Drain>
bool drainWhileConnected(IsConnected&& isConnected, Drain&& drain) {
    if (!isConnected() || !drain()) {
        return false;
    }
    return isConnected();
}

#endif  // COMMON_MQTT_DRAIN_HPP
