#include <gtest/gtest.h>

#include <cstdint>

#include "common/MqttDrain.hpp"

TEST(MqttDrainTest, ClosedSocketWithUnreadReceiveDataIsNotDrained) {
    const bool connectedWithUnreadReceiveData = true;
    const bool statusEstablished = false;

    EXPECT_TRUE(connectedWithUnreadReceiveData);
    EXPECT_FALSE(isTcpDrainComplete(statusEstablished, 1460, 1460));
}

TEST(MqttDrainTest, DrainNeedsEntireSendBufferAvailable) {
    EXPECT_FALSE(isTcpDrainComplete(true, 1459, 1460));
    EXPECT_TRUE(isTcpDrainComplete(true, 1460, 1460));
}

TEST(MqttDrainTest, ProgressingAcknowledgmentsDoNotExtendDeadline) {
    constexpr uint32_t startedAtMs = 100;
    constexpr uint32_t timeoutMs = 10;

    for (uint32_t elapsed = 1; elapsed < timeoutMs; ++elapsed) {
        EXPECT_FALSE(isTcpDrainComplete(true, elapsed, timeoutMs));
        EXPECT_FALSE(isDrainDeadlineExpired(
            startedAtMs, startedAtMs + elapsed, timeoutMs));
    }
    EXPECT_TRUE(isDrainDeadlineExpired(
        startedAtMs, startedAtMs + timeoutMs, timeoutMs));
}

TEST(MqttDrainTest, DeadlineHandlesMillisWraparound) {
    constexpr uint32_t startedAtMs = 0xfffffff0;

    EXPECT_FALSE(isDrainDeadlineExpired(startedAtMs, 0xfffffffe, 32));
    EXPECT_TRUE(isDrainDeadlineExpired(startedAtMs, 0x10, 32));
}
