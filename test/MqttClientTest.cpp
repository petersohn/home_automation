#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>

#include "DummyBackoff.hpp"
#include "EspTestBase.hpp"
#include "FakeMqttConnection.hpp"
#include "FakeOtaFileSystem.hpp"
#include "TestHelpers.hpp"
#include "common/ArduinoJson.hpp"
#include "common/ConfigBootCoordinator.hpp"
#include "common/CycleTimer.hpp"
#include "common/DeviceConfigOta.hpp"
#include "common/MqttClient.hpp"
#include "common/OtaLifecycleReadiness.hpp"
#include "common/OtaRestartGate.hpp"
#include "common/OtaResultPublisher.hpp"
#include "common/OtaTypes.hpp"
#include "common/UploadJournal.hpp"
#include "common/UploadStatus.hpp"
#include "tools/string.hpp"

using namespace ArduinoJson;

namespace {

size_t estimateEsp8266StatusJsonBufferUse(
    const std::string& name, const std::string& mac, const std::string& ip,
    const std::string& hash, const std::string& reason) {
    constexpr size_t pointerSize = 4;
    constexpr size_t objectSize = 2 * pointerSize;
    constexpr size_t variantSize = 2 * pointerSize;
    constexpr size_t nodeSize = 2 * pointerSize + variantSize;
    size_t used = 0;
    auto allocate = [&used](size_t bytes) {
        used = (used + 3) & ~size_t{3};
        used += bytes;
    };
    auto addField = [&allocate](const char* key, size_t valueBytes = 0) {
        allocate(nodeSize);
        allocate(std::strlen(key) + 1);
        if (valueBytes != 0) {
            allocate(valueBytes);
        }
    };

    allocate(objectSize);
    addField("name", name.size() + 1);
    addField("mac", mac.size() + 1);
    addField("restarted");
    addField("ip", ip.size() + 1);
    addField("uptime");
    addField("freeMemory");
    addField("avgCycleTime");
    addField("maxCycleTime");
    addField("configVersion");
    addField("minimumAcceptedConfigVersion", sizeof("null"));
    addField("trialState", sizeof("started"));
    allocate(objectSize);
    addField("lastConfigUpdate");
    addField("status", sizeof("failure"));
    addField("transferred");
    addField("hash", hash.size() + 1);
    addField("version", sizeof("null"));
    addField("reason", reason.size() + 1);
    return used;
}

size_t estimateEsp8266InboundStatusJsonBufferUse() {
    constexpr size_t pointerSize = 4;
    constexpr size_t objectSize = 2 * pointerSize;
    constexpr size_t variantSize = 2 * pointerSize;
    constexpr size_t nodeSize = 2 * pointerSize + variantSize;
    size_t used = objectSize;
    for (size_t field = 0; field < 11; ++field) {
        used = (used + 3) & ~size_t{3};
        used += nodeSize;
    }
    used = (used + 3) & ~size_t{3};
    used += nodeSize + objectSize;
    for (size_t field = 0; field < 5; ++field) {
        used = (used + 3) & ~size_t{3};
        used += nodeSize;
    }
    return used;
}

class RetryOnceMqttConnection : public FakeMqttConnection {
public:
    using FakeMqttConnection::FakeMqttConnection;

    bool subscribe(const char* topic, uint8_t qos = 0) override {
        if (this->failNextSubscription) {
            this->failNextSubscription = false;
            this->subscriptionHistory.emplace_back(topic, qos);
            return false;
        }
        return FakeMqttConnection::subscribe(topic, qos);
    }

private:
    bool failNextSubscription = true;
};

}  // namespace

struct StatusMessage {
    unsigned long time;
    bool restarted;
    unsigned long maxCycleTime;
    float avgCycleTime;

    StatusMessage(
        unsigned long time, bool restarted, unsigned long maxCycleTime,
        float avgCycleTime)
        : time(time)
        , restarted(restarted)
        , maxCycleTime(maxCycleTime)
        , avgCycleTime(avgCycleTime) {}
};

bool operator==(const StatusMessage& lhs, const StatusMessage& rhs) {
    return std::tie(lhs.time, lhs.restarted, lhs.maxCycleTime) ==
               std::tie(rhs.time, rhs.restarted, rhs.maxCycleTime) &&
           std::abs(lhs.avgCycleTime - rhs.avgCycleTime) < 0.01;
}

bool operator!=(const StatusMessage& lhs, const StatusMessage& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const StatusMessage& msg) {
    os << msg.time;
    if (msg.restarted) {
        os << " restarted";
    }
    os << " [max=" << msg.maxCycleTime << " avg=" << msg.avgCycleTime << "]";
    return os;
}

struct AvailabilityMessage {
    unsigned long time;
    bool isAvailable;

    AvailabilityMessage(unsigned long time, bool isAvailable)
        : time(time), isAvailable(isAvailable) {}
};

std::ostream& operator<<(std::ostream& os, const AvailabilityMessage& msg) {
    return os << msg.time << " -> " << msg.isAvailable;
}

bool operator==(
    const AvailabilityMessage& lhs, const AvailabilityMessage& rhs) {
    return std::tie(lhs.time, lhs.isAvailable) ==
           std::tie(rhs.time, rhs.isAvailable);
}

bool operator!=(
    const AvailabilityMessage& lhs, const AvailabilityMessage& rhs) {
    return !(lhs == rhs);
}

struct ConnectionAttempt {
    unsigned long time;
    bool success;

    ConnectionAttempt(unsigned long time, bool success)
        : time(time), success(success) {}
};

std::ostream& operator<<(std::ostream& os, const ConnectionAttempt& msg) {
    return os << msg.time << " -> " << msg.success;
}

bool operator==(const ConnectionAttempt& lhs, const ConnectionAttempt& rhs) {
    return std::tie(lhs.time, lhs.success) == std::tie(rhs.time, rhs.success);
}

bool operator!=(const ConnectionAttempt& lhs, const ConnectionAttempt& rhs) {
    return !(lhs == rhs);
}

class MqttClientTest : public EspTestBase {
public:
    FakeMqttServer server;
    FakeMqttConnection connection{this->server, [this](bool success) {
        this->connectionAttempts.emplace_back(this->esp.millis(), success);
    }};
    DummyBackoff backoff;
    CycleTimer cycleTimer{this->esp};
    MqttClient mqttClient{this->debug,   this->esp,        this->wifi,
                          this->backoff, this->connection, this->cycleTimer,
                          []() {}};

    size_t connectionId;
    std::vector<StatusMessage> statusMessages;
    std::vector<AvailabilityMessage> availabilityMessages;
    std::vector<ConnectionAttempt> connectionAttempts;
    std::vector<std::string> statusPayloads;
    const std::string deviceName = "this name should be way long enough to fit";

    MqttClientTest() {
        this->connectionId = this->server.connect({});
        this->server.subscribe(
            this->connectionId, "status",
            [this](size_t id, FakeMessage message) {
            if (id == this->connectionId) {
                return;
            }

            std::cout << message.payload << "\n";
            this->statusPayloads.push_back(message.payload);
            DynamicJsonBuffer buffer(1024);
            auto& json = buffer.parseObject(message.payload);
            auto name = json.get<std::string>("name");
            auto uptime = json.get<unsigned long>("uptime");
            auto restarted = json.get<bool>("restarted");
            auto maxCycleTime = json.get<unsigned long>("maxCycleTime");
            auto avgCycleTime = json.get<float>("avgCycleTime");

            EXPECT_EQ(uptime, this->esp.millis());
            EXPECT_EQ(name, this->deviceName);
            this->statusMessages.emplace_back(
                uptime, restarted, maxCycleTime, avgCycleTime);
        });
        this->server.subscribe(
            this->connectionId, "ava", [this](size_t id, FakeMessage message) {
            if (id == this->connectionId) {
                return;
            }

            bool isAvailable = false;
            auto res = tools::getBoolValue(
                message.payload.c_str(), isAvailable, message.payload.size());
            ASSERT_TRUE(res);
            this->availabilityMessages.emplace_back(
                this->esp.millis(), isAvailable);
        });

        this->mqttClient.setConfig(
            MqttConfig{this->deviceName, {ServerConfig{}}, {"ava", "status"}});
    }

    void loopUntil(unsigned long time, unsigned long delay = 100) {
        this->delayUntil(time, delay, [&]() {
            this->cycleTimer.tick();
            this->mqttClient.loop();
        });
    }

    void sendStatusMessage(const std::string& mac) {
        this->sendNamedStatusMessage(this->deviceName, mac);
    }

    void sendNamedStatusMessage(
        const std::string& name, const std::string& mac) {
        DynamicJsonBuffer buffer(200);
        JsonObject& message = buffer.createObject();
        message["name"] = name;
        message["mac"] = mac;
        std::string result;
        message.printTo(result);
        this->server.publish(
            this->connectionId, {"status", std::move(result), true});
    }

    void sendSameDeviceStatus() {
        this->sendStatusMessage(this->wifi.getMac());
    }

    void sendOtherDeviceStatus() {
        this->sendStatusMessage("11:11:11:11:11:11");
    }

    void sendAvailability(bool value) {
        this->server.publish(
            this->connectionId, {"ava", value ? "1" : "0", true});
    }

    void check(
        const std::vector<ConnectionAttempt>& expectedConnectionAttempts,
        const std::vector<StatusMessage>& expectedStatusMessages,
        const std::vector<AvailabilityMessage>& expectedAvailabilityMessages) {
        EXPECT_COLLECTIONS_EQ(
            this->connectionAttempts.begin(), this->connectionAttempts.end(),
            expectedConnectionAttempts.begin(),
            expectedConnectionAttempts.end());
        EXPECT_COLLECTIONS_EQ(
            this->statusMessages.begin(), this->statusMessages.end(),
            expectedStatusMessages.begin(), expectedStatusMessages.end());
        EXPECT_COLLECTIONS_EQ(
            this->availabilityMessages.begin(),
            this->availabilityMessages.end(),
            expectedAvailabilityMessages.begin(),
            expectedAvailabilityMessages.end());
    }
};

TEST_F(MqttClientTest, DrainOnlyCallsConnectedTransportAndReturnsResult) {
    EXPECT_FALSE(this->mqttClient.drain(1000));
    EXPECT_EQ(this->connection.drainCalls, 0u);

    this->mqttClient.loop();
    ASSERT_TRUE(this->connection.isConnected());

    this->connection.drainResult = false;
    EXPECT_FALSE(this->mqttClient.drain(1000));
    this->connection.drainResult = true;
    EXPECT_TRUE(this->mqttClient.drain(1000));
    EXPECT_EQ(this->connection.drainCalls, 2u);
    EXPECT_EQ(this->connection.lastDrainTimeoutMs, 1000u);
}

TEST_F(MqttClientTest, ClosedTransportDuringSuccessfulDrainCannotRestart) {
    OtaRestartGate gate;
    gate.recordStagedOutcome(true, false);
    gate.lifecycleResultSubmitted();
    gate.requestRestart(false);
    this->mqttClient.loop();
    this->connection.disconnectDuringDrain = true;

    gate.tcpDrainCompleted(this->mqttClient.drain(1000));

    EXPECT_FALSE(gate.takeRestart());
}

TEST_F(MqttClientTest, NormalFlow) {
    this->loopUntil(100000);
    this->connection.disconnect();
    this->loopUntil(110000);

    this->check(
        {{100, true}, {100100, true}},
        {
            {2100, true, 100, 100.0},
            {62100, false, 100, 100.0},
            {100200, false, 100, 100.0},
        },
        {{2100, true}, {62100, true}, {100000, false}, {100200, true}});
}

TEST_F(MqttClientTest, RetryConnection) {
    this->server.working = false;
    this->loopUntil(200000);
    this->server.working = true;
    this->loopUntil(250000);

    this->check(
        {{100, false},
         {600, false},
         {1600, false},
         {3600, false},
         {7600, false},
         {15600, false},
         {31600, false},
         {63600, false},
         {123600, false},
         {183600, false},
         {243600, true}},
        {{245600, true, 100, 100.0}}, {{245600, true}});
}

TEST_F(MqttClientTest, ConnectionFromSameDevice_Available_StatusFirst) {
    this->loopUntil(20, 5);
    this->sendSameDeviceStatus();
    this->loopUntil(40, 5);
    this->sendAvailability(true);
    this->loopUntil(60, 5);

    EXPECT_TRUE(this->connection.isConnected());
    this->check({{5, true}}, {{30, true, 5, 5.0}}, {{30, true}});
}

TEST_F(MqttClientTest, ConnectionFromSameDevice_NotAvailable_StatusFirst) {
    this->loopUntil(20, 5);
    this->sendSameDeviceStatus();
    this->loopUntil(40, 5);
    this->sendAvailability(false);
    this->loopUntil(60, 5);

    EXPECT_TRUE(this->connection.isConnected());
    this->check(
        {{5, true}}, {{30, true, 5, 5.0}, {50, false, 5, 5.0}},
        {{30, true}, {50, true}});
}

TEST_F(MqttClientTest, ConnectionFromSameDevice_Available_AvailabilityFirst) {
    this->loopUntil(20, 5);
    this->sendAvailability(true);
    this->loopUntil(40, 5);
    this->sendSameDeviceStatus();
    this->loopUntil(60, 5);

    EXPECT_TRUE(this->connection.isConnected());
    this->check({{5, true}}, {{50, true, 5, 5.0}}, {{50, true}});
}

TEST_F(
    MqttClientTest, ConnectionFromSameDevice_NotAvailable_AvailabilityFirst) {
    this->loopUntil(20, 5);
    this->sendAvailability(false);
    this->loopUntil(40, 5);
    this->sendSameDeviceStatus();
    this->loopUntil(60, 5);

    EXPECT_TRUE(this->connection.isConnected());
    this->check({{5, true}}, {{30, true, 5, 5.0}}, {{30, true}});
}

TEST_F(MqttClientTest, ConnectionFromOtherDevice_Available_StatusFirst) {
    this->loopUntil(20, 5);
    this->sendOtherDeviceStatus();
    this->loopUntil(40, 5);
    this->sendAvailability(true);
    this->loopUntil(60, 5);

    EXPECT_FALSE(this->connection.isConnected());
    this->check({{5, true}}, {}, {{45, false}});
}

TEST_F(MqttClientTest, ConnectionFromOtherDevice_NotAvailable_StatusFirst) {
    this->loopUntil(20, 5);
    this->sendOtherDeviceStatus();
    this->loopUntil(40, 5);
    this->sendAvailability(false);
    this->loopUntil(60, 5);

    EXPECT_TRUE(this->connection.isConnected());
    this->check({{5, true}}, {{50, true, 5, 5.0}}, {{50, true}});
}

TEST_F(MqttClientTest, ConnectionFromOtherDevice_Available_AvailabilityFirst) {
    this->loopUntil(20, 5);
    this->sendAvailability(true);
    this->loopUntil(40, 5);
    this->sendOtherDeviceStatus();
    this->loopUntil(60, 5);

    EXPECT_FALSE(this->connection.isConnected());
    this->check({{5, true}}, {}, {{45, false}});
}

TEST_F(
    MqttClientTest, ConnectionFromOtherDevice_NotAvailable_AvailabilityFirst) {
    this->loopUntil(20, 5);
    this->sendAvailability(false);
    this->loopUntil(40, 5);
    this->sendOtherDeviceStatus();
    this->loopUntil(60, 5);

    EXPECT_TRUE(this->connection.isConnected());
    this->check(
        {{5, true}}, {{30, true, 5, 5.0}, {50, false, 5, 5.0}},
        {{30, true}, {50, true}});
}

TEST_F(MqttClientTest, Subscribe) {
    std::string received;

    this->mqttClient.subscribe(
        "someTopic", [&](const MqttConnection::Message& message) {
        received = std::string{message.payload, message.payloadLength};
    });
    this->sendAvailability(false);
    this->mqttClient.loop();
    this->esp.delay(100);

    this->server.publish(
        this->connectionId, FakeMessage{"otherTopic", "payload1"});
    this->mqttClient.loop();
    EXPECT_EQ(received, "");

    this->server.publish(
        this->connectionId, FakeMessage{"someTopic", "payload2"});
    this->mqttClient.loop();
    EXPECT_EQ(received, "payload2");

    this->server.publish(
        this->connectionId, FakeMessage{"otherTopic", "payload3"});
    this->mqttClient.loop();
    EXPECT_EQ(received, "payload2");

    this->server.publish(
        this->connectionId, FakeMessage{"someTopic", "payload4"});
    this->mqttClient.loop();
    EXPECT_EQ(received, "payload4");

    this->esp.delay(100000);
    this->mqttClient.loop();
    EXPECT_EQ(received, "payload4");
}

TEST_F(MqttClientTest, PrioritySubscriptionWinsDuplicateTopic) {
    constexpr const char* topic = "home/update/device-config";
    size_t interfaceMessages = 0;
    std::string otaPayload;

    this->mqttClient.subscribe(
        topic, [&interfaceMessages](const MqttConnection::Message&) {
        ++interfaceMessages;
    });
    this->mqttClient.subscribePriority(
        topic, [&otaPayload](const MqttConnection::Message& message) {
        otaPayload.assign(message.payload, message.payloadLength);
    }, 1);
    this->mqttClient.loop();

    ASSERT_TRUE(this->mqttClient.subscriptionReady(topic));
    this->server.publish(this->connectionId, FakeMessage{topic, "ota-command"});
    this->mqttClient.loop();

    EXPECT_EQ(interfaceMessages, 0u);
    EXPECT_EQ(otaPayload, "ota-command");
}

TEST_F(MqttClientTest, SubscriptionQosRestoredAfterReconnect) {
    const auto callback = [](const MqttConnection::Message&) {};
    this->mqttClient.subscribe("defaultQos", callback);
    this->mqttClient.subscribe("requestedQos", callback, 1);
    this->mqttClient.subscribe("invalidQos", callback, 2);

    this->mqttClient.loop();
    this->connection.disconnect();
    this->mqttClient.loop();

    auto count = [this](const std::string& topic, uint8_t qos) {
        return std::count(
            this->connection.subscriptionHistory.begin(),
            this->connection.subscriptionHistory.end(),
            std::make_pair(topic, qos));
    };
    EXPECT_EQ(count("defaultQos", 0), 2);
    EXPECT_EQ(count("requestedQos", 1), 2);
    EXPECT_EQ(count("invalidQos", 2), 0);
}

TEST_F(MqttClientTest, WifiLossInvalidatesWithoutLoop) {
    this->mqttClient.loop();
    const auto before = this->mqttClient.getTransportState();
    ASSERT_TRUE(before.connected);
    this->wifi.status = WifiStatus::Disconnected;

    const auto after = this->mqttClient.getTransportState();
    EXPECT_FALSE(after.connected);
    EXPECT_NE(after.generation, before.generation);
}

TEST_F(MqttClientTest, ReplacementConnectionChangesGeneration) {
    this->mqttClient.loop();
    const auto before = this->mqttClient.getTransportState();
    ASSERT_TRUE(before.connected);

    this->connection.disconnect();
    this->mqttClient.loop();
    const auto after = this->mqttClient.getTransportState();
    EXPECT_TRUE(after.connected);
    EXPECT_NE(after.generation, before.generation);
}

TEST_F(
    MqttClientTest,
    InitializedReplacementChangesGenerationBeforeFirstCallback) {
    this->mqttClient.loop();
    this->sendAvailability(false);
    this->mqttClient.loop();
    this->mqttClient.loop();
    ASSERT_FALSE(this->statusMessages.empty());
    const auto before = this->mqttClient.getTransportState();
    ASSERT_TRUE(before.connected);

    bool received = false;
    this->mqttClient.subscribe(
        "session",
        [this, &before, &received](const MqttConnection::Message& message) {
        received = true;
        const auto state = this->mqttClient.getTransportState();
        EXPECT_TRUE(state.connected);
        EXPECT_NE(state.generation, before.generation);
        EXPECT_EQ(
            (std::string{message.payload, message.payloadLength}),
            "new-session");
    });
    this->server.publish(
        this->connectionId, FakeMessage{"session", "new-session", true});
    this->connection.disconnect();

    this->mqttClient.loop();
    EXPECT_TRUE(received);
}

TEST_F(MqttClientTest, ExplicitDisconnectInvalidatesTransportState) {
    this->mqttClient.loop();
    const auto before = this->mqttClient.getTransportState();
    ASSERT_TRUE(before.connected);

    this->mqttClient.disconnect();
    const auto after = this->mqttClient.getTransportState();
    EXPECT_FALSE(after.connected);
    EXPECT_NE(after.generation, before.generation);
}

TEST_F(MqttClientTest, FailedAttemptsAndStableQueriesDoNotChangeGeneration) {
    this->server.working = false;
    this->mqttClient.loop();
    const auto initial = this->mqttClient.getTransportState();
    EXPECT_FALSE(initial.connected);

    EXPECT_EQ(
        this->mqttClient.getTransportState().generation, initial.generation);
    this->esp.delay(1000);
    this->mqttClient.loop();
    EXPECT_EQ(
        this->mqttClient.getTransportState().generation, initial.generation);
}

TEST_F(MqttClientTest, DisconnectClearsQueuedMessagesAndSessionSubscriptions) {
    size_t received = 0;
    this->mqttClient.subscribe(
        "someTopic",
        [&received](const MqttConnection::Message&) { ++received; });
    this->mqttClient.loop();

    this->server.publish(this->connectionId, {"someTopic", "old-session"});
    this->connection.disconnect();
    this->mqttClient.loop();
    EXPECT_EQ(received, 0);

    this->server.publish(this->connectionId, {"someTopic", "new-session"});
    this->mqttClient.loop();
    EXPECT_EQ(received, 1);
}

TEST_F(MqttClientTest, AvailabilityCollisionInvalidatesTransportState) {
    this->mqttClient.loop();
    const auto before = this->mqttClient.getTransportState();
    ASSERT_TRUE(before.connected);

    this->sendAvailability(true);
    this->mqttClient.loop();
    this->sendOtherDeviceStatus();
    this->mqttClient.loop();

    const auto after = this->mqttClient.getTransportState();
    EXPECT_FALSE(after.connected);
    EXPECT_NE(after.generation, before.generation);
}

TEST_F(MqttClientTest, DifferentNameSameStatusTopicCollides) {
    this->mqttClient.loop();
    ASSERT_TRUE(this->mqttClient.isConnected());
    this->sendAvailability(true);
    this->mqttClient.loop();

    this->sendNamedStatusMessage("another device", "11:11:11:11:11:11");
    this->mqttClient.loop();

    EXPECT_FALSE(this->mqttClient.isConnected());
    EXPECT_EQ(this->mqttClient.getReadiness(), MqttReadiness::collision);
    EXPECT_TRUE(this->mqttClient.takeCollision());
}

TEST_F(MqttClientTest, MalformedMacCannotConfirmSelf) {
    this->wifi.mac = "a1:B2:c3:D4:e5:F6";
    this->mqttClient.loop();
    this->sendAvailability(true);
    this->mqttClient.loop();

    this->sendStatusMessage("a1:B2:c3:D4:e5");
    this->mqttClient.loop();
    EXPECT_EQ(this->mqttClient.getReadiness(), MqttReadiness::checking);
    EXPECT_FALSE(this->mqttClient.takeCollision());

    std::string caseVariant = this->wifi.mac;
    std::transform(
        caseVariant.begin(), caseVariant.end(), caseVariant.begin(),
        [](unsigned char value) {
        return static_cast<char>(std::toupper(value));
    });
    this->sendStatusMessage(caseVariant);
    this->mqttClient.loop();
    EXPECT_EQ(this->mqttClient.getReadiness(), MqttReadiness::ready);
}

TEST_F(MqttClientTest, TransportConnectionIsNotReadiness) {
    this->mqttClient.loop();

    ASSERT_TRUE(this->mqttClient.isConnected());
    EXPECT_EQ(this->mqttClient.getReadiness(), MqttReadiness::checking);

    this->sendAvailability(false);
    this->mqttClient.loop();
    EXPECT_EQ(this->mqttClient.getReadiness(), MqttReadiness::ready);
}

TEST_F(MqttClientTest, CollisionLatchSurvivesReconnectBookkeeping) {
    this->mqttClient.loop();
    this->sendAvailability(true);
    this->mqttClient.loop();
    this->sendNamedStatusMessage("different name", "11:11:11:11:11:11");
    this->mqttClient.loop();

    EXPECT_EQ(this->mqttClient.getReadiness(), MqttReadiness::collision);
    this->server.publish(this->connectionId, {"ava", "", true});
    this->server.publish(this->connectionId, {"status", "", true});
    this->esp.delay(500);
    this->mqttClient.loop();
    EXPECT_TRUE(this->mqttClient.isConnected());
    EXPECT_EQ(this->mqttClient.getReadiness(), MqttReadiness::checking);
    this->mqttClient.disconnect();
    EXPECT_TRUE(this->mqttClient.takeCollision());
    EXPECT_FALSE(this->mqttClient.takeCollision());
}

TEST_F(MqttClientTest, FailedUpdateSubscriptionCannotConfirmUsableOta) {
    constexpr const char* updateTopic = "home/device/update/device-config";
    this->mqttClient.subscribe(
        updateTopic, [](const MqttConnection::Message&) {}, 2);
    this->mqttClient.loop();

    EXPECT_EQ(this->mqttClient.getReadiness(), MqttReadiness::checking);
    EXPECT_FALSE(this->mqttClient.subscriptionReady(updateTopic));

    this->sendAvailability(true);
    this->mqttClient.loop();
    this->sendNamedStatusMessage("foreign name", "11:11:11:11:11:11");
    this->mqttClient.loop();

    EXPECT_TRUE(this->mqttClient.takeCollision());
    EXPECT_FALSE(this->mqttClient.takeCollision());
    EXPECT_FALSE(this->mqttClient.subscriptionReady(updateTopic));
}

TEST_F(MqttClientTest, SubscriptionReadinessTracksImmediateAndReconnect) {
    constexpr const char* reconnectTopic = "device/reconnect";
    constexpr const char* immediateTopic = "device/immediate";
    const auto callback = [](const MqttConnection::Message&) {};
    this->mqttClient.subscribe(reconnectTopic, callback);
    this->mqttClient.loop();

    ASSERT_TRUE(this->mqttClient.subscriptionReady(reconnectTopic));
    this->mqttClient.subscribe(immediateTopic, callback, 1);
    EXPECT_TRUE(this->mqttClient.subscriptionReady(immediateTopic));

    const auto before = this->mqttClient.getTransportState();
    this->mqttClient.disconnect();
    EXPECT_FALSE(this->mqttClient.subscriptionReady(reconnectTopic));
    EXPECT_FALSE(this->mqttClient.subscriptionReady(immediateTopic));

    this->mqttClient.loop();
    const auto after = this->mqttClient.getTransportState();
    EXPECT_NE(after.generation, before.generation);
    EXPECT_TRUE(this->mqttClient.subscriptionReady(reconnectTopic));
    EXPECT_TRUE(this->mqttClient.subscriptionReady(immediateTopic));
}

TEST_F(MqttClientTest, FailedSubscriptionRetriesAtOneSecondIntervals) {
    FakeMqttServer server;
    RetryOnceMqttConnection connection(server, [](bool) {});
    CycleTimer cycleTimer(this->esp);
    MqttClient client(
        this->debug, this->esp, this->wifi, this->backoff, connection,
        cycleTimer, []() {});
    client.setConfig(MqttConfig{"retry-client", {ServerConfig{}}, {"", ""}});
    constexpr const char* topic = "device/retry";
    client.loop();
    client.subscribe(topic, [](const MqttConnection::Message&) {});

    ASSERT_EQ(connection.subscriptionHistory.size(), 1u);
    EXPECT_FALSE(client.subscriptionReady(topic));
    this->delayUntil(900, 100, [&client]() { client.loop(); });
    EXPECT_EQ(connection.subscriptionHistory.size(), 1u);
    this->delayUntil(1000, 100, [&client]() { client.loop(); });
    EXPECT_EQ(connection.subscriptionHistory.size(), 2u);
    EXPECT_TRUE(client.subscriptionReady(topic));
    this->delayUntil(2000, 100, [&client]() { client.loop(); });
    EXPECT_EQ(connection.subscriptionHistory.size(), 2u);
}

TEST_F(MqttClientTest, LiveDisconnectClearsReadinessBeforeLoop) {
    constexpr const char* topic = "device/live-disconnect";
    this->sendAvailability(false);
    this->mqttClient.loop();
    this->mqttClient.subscribe(topic, [](const MqttConnection::Message&) {});

    ASSERT_EQ(this->mqttClient.getReadiness(), MqttReadiness::ready);
    ASSERT_TRUE(this->mqttClient.subscriptionReady(topic));
    this->connection.disconnect();

    EXPECT_EQ(this->mqttClient.getReadiness(), MqttReadiness::checking);
    EXPECT_FALSE(this->mqttClient.subscriptionReady(topic));
}

TEST_F(MqttClientTest, LiveWifiLossClearsReadinessBeforeLoop) {
    constexpr const char* topic = "device/live-wifi-loss";
    this->sendAvailability(false);
    this->mqttClient.loop();
    this->mqttClient.subscribe(topic, [](const MqttConnection::Message&) {});

    ASSERT_EQ(this->mqttClient.getReadiness(), MqttReadiness::ready);
    ASSERT_TRUE(this->mqttClient.subscriptionReady(topic));
    this->wifi.status = WifiStatus::Disconnected;

    EXPECT_EQ(this->mqttClient.getReadiness(), MqttReadiness::checking);
    EXPECT_FALSE(this->mqttClient.subscriptionReady(topic));
}

TEST_F(MqttClientTest, FailedSubscriptionRetriesAfterOneSecondUntilReady) {
    FakeMqttServer server;
    RetryOnceMqttConnection connection(server, [](bool) {});
    CycleTimer cycleTimer(this->esp);
    MqttClient client(
        this->debug, this->esp, this->wifi, this->backoff, connection,
        cycleTimer, []() {});
    constexpr const char* topic = "device/retry-once";
    client.setConfig(MqttConfig{"retry-client", {ServerConfig{}}, {"", ""}});

    client.loop();
    client.subscribe(topic, [](const MqttConnection::Message&) {});
    ASSERT_EQ(connection.subscriptionHistory.size(), 1u);
    EXPECT_FALSE(client.subscriptionReady(topic));

    this->delayUntil(900, 100, [&client]() { client.loop(); });
    EXPECT_EQ(connection.subscriptionHistory.size(), 1u);
    EXPECT_FALSE(client.subscriptionReady(topic));

    this->delayUntil(1000, 100, [&client]() { client.loop(); });
    ASSERT_EQ(connection.subscriptionHistory.size(), 2u);
    EXPECT_TRUE(client.subscriptionReady(topic));

    this->delayUntil(1900, 100, [&client]() { client.loop(); });
    EXPECT_EQ(connection.subscriptionHistory.size(), 2u);
}

TEST_F(
    MqttClientTest,
    FailedOrdinaryDuplicateRetryCannotReplacePrioritySubscription) {
    FakeMqttServer server;
    const size_t publisherId = server.connect({});
    RetryOnceMqttConnection connection(server, [](bool) {});
    CycleTimer cycleTimer(this->esp);
    MqttClient client(
        this->debug, this->esp, this->wifi, this->backoff, connection,
        cycleTimer, []() {});
    constexpr const char* topic = "home/update/device-config";
    size_t ordinaryMessages = 0;
    std::vector<std::string> priorityMessages;
    client.setConfig(MqttConfig{"retry-client", {ServerConfig{}}, {"", ""}});

    client.loop();
    client.subscribe(
        topic, [&ordinaryMessages](const MqttConnection::Message&) {
        ++ordinaryMessages;
    });
    client.subscribePriority(
        topic, [&priorityMessages](const MqttConnection::Message& message) {
        priorityMessages.emplace_back(message.payload, message.payloadLength);
    }, 1);

    ASSERT_EQ(
        connection.subscriptionHistory,
        (std::vector<std::pair<std::string, uint8_t>>{{topic, 0}, {topic, 1}}));
    ASSERT_TRUE(client.subscriptionReady(topic));

    this->delayUntil(1000, 100, [&client]() { client.loop(); });

    EXPECT_EQ(
        connection.subscriptionHistory,
        (std::vector<std::pair<std::string, uint8_t>>{{topic, 0}, {topic, 1}}));
    EXPECT_TRUE(client.subscriptionReady(topic));

    server.publish(publisherId, FakeMessage{topic, "ota-command"});
    client.loop();
    EXPECT_EQ(ordinaryMessages, 0u);
    EXPECT_EQ(priorityMessages, (std::vector<std::string>{"ota-command"}));

    const auto prioritySubscribes = std::count(
        connection.subscriptionHistory.begin(),
        connection.subscriptionHistory.end(),
        std::make_pair(std::string{topic}, uint8_t{1}));
    connection.disconnect();
    client.loop();

    const auto reassertedPrioritySubscribes = std::count(
        connection.subscriptionHistory.begin(),
        connection.subscriptionHistory.end(),
        std::make_pair(std::string{topic}, uint8_t{1}));
    EXPECT_EQ(reassertedPrioritySubscribes, prioritySubscribes + 1);
    EXPECT_EQ(
        connection.subscriptionHistory.back(),
        std::make_pair(std::string{topic}, uint8_t{1}));
    EXPECT_TRUE(client.subscriptionReady(topic));
}

TEST_F(MqttClientTest, StatusPacketReservesPubSubClientSevenByteOverhead) {
    constexpr const char* topic = "boundary";
    std::vector<std::string> payloads;
    ASSERT_TRUE(this->server.subscribe(
        this->connectionId, topic,
        [&payloads, this](size_t id, FakeMessage message) {
        if (id != this->connectionId) {
            payloads.push_back(std::move(message.payload));
        }
    }));
    this->mqttClient.setConfig(
        MqttConfig{this->deviceName, {ServerConfig{}}, {"ava", topic}});

    std::string reason;
    this->mqttClient.setOtaStatusProvider([&reason]() {
        OtaStatusSnapshot snapshot;
        snapshot.lastOutcome =
            OtaResult{"failure", 0, "", std::nullopt, reason};
        return snapshot;
    });
    this->sendAvailability(false);
    this->loopUntil(20, 5);
    ASSERT_EQ(payloads.size(), 1u);

    constexpr size_t packetBufferSize = 1024;
    constexpr size_t pubSubMaxHeaderSize = 5;
    constexpr size_t topicLengthFieldSize = 2;
    constexpr size_t pubSubClientOverhead =
        pubSubMaxHeaderSize + topicLengthFieldSize;
    const size_t compliantPayloadLimit =
        packetBufferSize - std::strlen(topic) - pubSubClientOverhead;
    EXPECT_LE(
        payloads.front().size() + std::strlen(topic) + pubSubClientOverhead,
        packetBufferSize);

    const size_t sixByteAllowanceBoundary =
        packetBufferSize - std::strlen(topic) - 6;
    ASSERT_LT(payloads.front().size(), sixByteAllowanceBoundary);
    reason.assign(sixByteAllowanceBoundary - payloads.front().size(), 'r');
    ASSERT_EQ(
        payloads.front().size() + reason.size(), sixByteAllowanceBoundary);
    EXPECT_LT(compliantPayloadLimit, sixByteAllowanceBoundary);

    this->loopUntil(60005, 1000);
    EXPECT_EQ(payloads.size(), 1u);
}

TEST_F(MqttClientTest, OtaStatusAllocationFailureDoesNotPublishNullReason) {
    this->mqttClient.setOtaStatusProvider([]() {
        OtaStatusSnapshot snapshot;
        snapshot.lastOutcome =
            OtaResult{"failure", 0, "", std::nullopt, std::string(2048, 'r')};
        return snapshot;
    });
    this->sendAvailability(false);
    this->loopUntil(20, 5);

    EXPECT_TRUE(this->statusPayloads.empty());
}

TEST_F(MqttClientTest, OtaStatusFitsDevicePacketAndParsesWithNullMinimum) {
    this->mqttClient.setOtaStatusProvider([]() {
        OtaStatusSnapshot snapshot;
        snapshot.configVersion = std::numeric_limits<uint32_t>::max();
        snapshot.minimumAcceptedConfigVersion.reset();
        snapshot.phase = UploadPhase::started;
        snapshot.updatesAllowed = false;
        snapshot.lastOutcome = OtaResult{
            "failure", std::numeric_limits<uint32_t>::max(),
            std::string(64, 'a'), std::numeric_limits<uint32_t>::max(),
            std::string(256, 'r')};
        return snapshot;
    });

    this->sendAvailability(false);
    this->loopUntil(20, 5);
    ASSERT_FALSE(this->statusPayloads.empty());
    const std::string& payload = this->statusPayloads.front();
    DynamicJsonBuffer buffer(2048);
    auto& status = buffer.parseObject(payload);

    EXPECT_TRUE(status.containsKey("configVersion"));
    EXPECT_EQ(
        status.get<uint32_t>("configVersion"),
        std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(status.get<std::string>("trialState"), "started");
    EXPECT_NE(
        payload.find("\"minimumAcceptedConfigVersion\":null"),
        std::string::npos);
    EXPECT_NE(payload.find("\"lastConfigUpdate\":{"), std::string::npos);
    EXPECT_NE(payload.find("\"status\":\"failure\""), std::string::npos);
    EXPECT_NE(payload.find(std::string(256, 'r')), std::string::npos);
    EXPECT_LE(payload.size() + std::string("status").size(), 1024u);

    const size_t outboundDeviceJsonUse = estimateEsp8266StatusJsonBufferUse(
        this->deviceName, this->wifi.mac, this->wifi.ip, std::string(64, 'a'),
        std::string(256, 'r'));
    EXPECT_LE(outboundDeviceJsonUse, 1536u);
    EXPECT_LE(estimateEsp8266InboundStatusJsonBufferUse(), 1536u);

    this->mqttClient.disconnect();
    this->sendAvailability(true);
    this->mqttClient.loop();
    EXPECT_EQ(this->mqttClient.getReadiness(), MqttReadiness::ready);
}

TEST_F(MqttClientTest, NoOtaProviderPreservesExistingStatus) {
    this->sendAvailability(false);
    this->loopUntil(20, 5);
    ASSERT_FALSE(this->statusPayloads.empty());
    DynamicJsonBuffer buffer(1024);
    auto& status = buffer.parseObject(this->statusPayloads.front());

    EXPECT_TRUE(status.containsKey("name"));
    EXPECT_TRUE(status.containsKey("mac"));
    EXPECT_TRUE(status.containsKey("uptime"));
    EXPECT_FALSE(status.containsKey("configVersion"));
    EXPECT_FALSE(status.containsKey("lastConfigUpdate"));
}

TEST_F(
    MqttClientTest,
    RetainedAvailabilityWaitsForInitializationBeforeTrialConfirmation) {
    constexpr const char* resultTopic = "home/update/device-config/result";
    constexpr const char* commandTopic = "home/update/device-config";
    const std::string hash(64, 'a');

    FakeOtaFileSystem fileSystem;
    fileSystem.seed(
        "/device_config.json",
        R"({"configVersion":1,"otaEnabled":true,"servicePrefix":"home"})");
    fileSystem.seed(
        "/device_config_2.json",
        R"({"configVersion":2,"otaEnabled":true,"servicePrefix":"home"})");
    UploadStatus pending;
    pending.sequence = 1;
    pending.activeVersion = 1;
    pending.phase = UploadPhase::pending;
    pending.trialSlot = ConfigSlot::secondary;
    pending.trialVersion = 2;
    pending.trialHash = hash;
    pending.trialBytes = 128;
    pending.minimumAcceptedConfigVersion = 3;
    pending.lastOutcome = OtaResult{"STAGED", 128, hash, 2, ""};
    const std::string pendingBytes = encodeUploadStatus(pending);
    fileSystem.seed("/upload_status.json", pendingBytes);
    fileSystem.seed("/upload_status_backup.json", pendingBytes);

    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);
    const ConfigBootSelection selection = boot.selectForBoot();
    ASSERT_TRUE(selection.trial);
    ASSERT_TRUE(boot.trialRunning());
    ASSERT_EQ(boot.snapshot().phase, UploadPhase::started);

    FakeMqttServer server;
    const size_t publisherId = server.connect({});
    server.publish(publisherId, FakeMessage{"ava", "0", true});
    const size_t observerId = server.connect({});
    std::vector<FakeMessage> results;
    ASSERT_TRUE(server.subscribe(
        observerId, resultTopic, [&results](size_t, FakeMessage message) {
        results.push_back(std::move(message));
    }));

    std::vector<std::string> events;
    size_t connectedCallbacks = 0;
    FakeMqttConnection connection(server, [](bool) {});
    CycleTimer timer(this->esp);
    MqttClient client(
        this->debug, this->esp, this->wifi, this->backoff, connection, timer,
        [&events, &connectedCallbacks]() {
        ++connectedCallbacks;
        events.push_back("connected");
    });
    client.setConfig(
        MqttConfig{"trial-device", {ServerConfig{}}, {"ava", "status"}});
    client.subscribe(commandTopic, [](const MqttConnection::Message&) {}, 1);

    OtaResultPublisher resultPublisher;
    DeviceConfigOta ota(
        this->esp, fileSystem, boot, nullptr, this->wifi.getMac(), [&client]() {
        return client.getTransportState();
    }, [&resultPublisher, &events](const OtaResult& result) {
        events.push_back("queued:" + result.status);
        resultPublisher.report(result);
    });

    client.loop();
    ASSERT_EQ(client.getReadiness(), MqttReadiness::ready);
    ASSERT_TRUE(client.isConnected());
    ASSERT_TRUE(client.subscriptionReady(commandTopic));
    EXPECT_EQ(connectedCallbacks, 0u);
    EXPECT_FALSE(client.canPublish());
    EXPECT_FALSE(client.tryPublish(resultTopic, "{}", false));

    OtaLifecycleReadiness lifecycleReadiness{
        client.getReadiness() == MqttReadiness::ready,
        client.canPublish(),
        true,
        true,
        true,
        client.subscriptionReady(commandTopic)};
    EXPECT_FALSE(isOtaLifecycleReady(lifecycleReadiness));
    ota.observeMqtt(
        isOtaLifecycleReady(lifecycleReadiness) ? MqttReadiness::ready
                                                : MqttReadiness::checking,
        client.takeCollision());
    ota.update();
    resultPublisher.flush([&client, resultTopic](const OtaResult& result) {
        const std::string payload = encodeOtaResult(result);
        return client.tryPublish(resultTopic, payload.c_str(), false);
    });

    EXPECT_TRUE(boot.trialRunning());
    EXPECT_EQ(boot.snapshot().phase, UploadPhase::started);
    EXPECT_EQ(journal.state()->activeVersion, 1u);
    EXPECT_EQ(boot.snapshot().configVersion, 2u);
    EXPECT_TRUE(results.empty());

    client.loop();
    ASSERT_EQ(connectedCallbacks, 1u);
    ASSERT_TRUE(client.canPublish());
    lifecycleReadiness.availabilityReady =
        client.getReadiness() == MqttReadiness::ready;
    lifecycleReadiness.canPublish = client.canPublish();
    lifecycleReadiness.commandSubscriptionReady =
        client.subscriptionReady(commandTopic);
    ASSERT_TRUE(isOtaLifecycleReady(lifecycleReadiness));
    ota.observeMqtt(
        isOtaLifecycleReady(lifecycleReadiness) ? MqttReadiness::ready
                                                : MqttReadiness::checking,
        client.takeCollision());
    ota.update();
    ASSERT_FALSE(boot.trialRunning());
    ASSERT_EQ(boot.snapshot().phase, UploadPhase::idle);
    ASSERT_EQ(boot.snapshot().configVersion, 2u);
    ASSERT_TRUE(boot.snapshot().lastOutcome.has_value());
    ASSERT_EQ(boot.snapshot().lastOutcome->status, "CONFIRMED");

    resultPublisher.flush(
        [&client, resultTopic, &events](const OtaResult& result) {
        const std::string payload = encodeOtaResult(result);
        const bool accepted =
            client.tryPublish(resultTopic, payload.c_str(), false);
        if (accepted) {
            events.push_back("submitted:" + result.status);
        }
        return accepted;
    });
    resultPublisher.flush([&client, resultTopic](const OtaResult& result) {
        const std::string payload = encodeOtaResult(result);
        return client.tryPublish(resultTopic, payload.c_str(), false);
    });

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results.front().retain);
    EXPECT_NE(
        results.front().payload.find("\"status\":\"CONFIRMED\""),
        std::string::npos);
    const auto connectedEvent =
        std::find(events.begin(), events.end(), "connected");
    const auto confirmedEvent =
        std::find(events.begin(), events.end(), "queued:CONFIRMED");
    const auto submittedEvent =
        std::find(events.begin(), events.end(), "submitted:CONFIRMED");
    ASSERT_NE(connectedEvent, events.end());
    ASSERT_NE(confirmedEvent, events.end());
    ASSERT_NE(submittedEvent, events.end());
    EXPECT_LT(connectedEvent, confirmedEvent);
    EXPECT_LT(confirmedEvent, submittedEvent);
}

TEST_F(MqttClientTest, CycleTime) {
    this->sendAvailability(false);
    this->loopUntil(20, 10);
    this->loopUntil(40020, 1000);
    this->loopUntil(60020, 500);

    this->check(
        {{10, true}},
        {
            {20, true, 10, 10.0},
            // Status at t=20 resets the timer; ticks at 1020..40020 (1000ms)
            // and 40520..60020 (500ms) give 80 ticks -> 79 intervals over
            // 59000ms, so the avg is 59000/79 (~746.8).
            {60020, false, 1000, 59000.0 / 79},
        },
        {{20, true}, {60020, true}});
}
