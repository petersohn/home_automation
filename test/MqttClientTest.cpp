#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

#include "DummyBackoff.hpp"
#include "EspTestBase.hpp"
#include "FakeMqttConnection.hpp"
#include "common/ArduinoJson.hpp"
#include "common/MqttClient.hpp"
#include "tools/string.hpp"

using namespace ArduinoJson;

BOOST_AUTO_TEST_SUITE(MqttClientTest)

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

class Fixture : public EspTestBase {
public:
    FakeMqttServer server;
    FakeMqttConnection connection{this->server, [this](bool success) {
        this->connectionAttempts.emplace_back(this->esp.millis(), success);
    }};
    DummyBackoff backoff;
    MqttClient mqttClient{this->debug,   this->esp,        this->wifi,
                          this->backoff, this->connection, []() {}};

    size_t connectionId;
    std::vector<StatusMessage> statusMessages;
    std::vector<AvailabilityMessage> availabilityMessages;
    std::vector<ConnectionAttempt> connectionAttempts;
    const std::string deviceName = "this name should be way long enough to fit";

    Fixture() {
        this->connectionId = this->server.connect({});
        this->server.subscribe(
            this->connectionId, "status",
            [this](size_t id, FakeMessage message) {
            if (id == this->connectionId) {
                return;
            }

            BOOST_TEST_MESSAGE(message.payload);
            DynamicJsonBuffer buffer(200);
            auto& json = buffer.parseObject(message.payload);
            auto name = json.get<std::string>("name");
            auto uptime = json.get<unsigned long>("uptime");
            auto restarted = json.get<bool>("restarted");
            auto maxCycleTime = json.get<unsigned long>("maxCycleTime");
            auto avgCycleTime = json.get<float>("avgCycleTime");

            BOOST_TEST(uptime == this->esp.millis());
            BOOST_TEST(name == this->deviceName);
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
            BOOST_TEST_REQUIRE(res);
            this->availabilityMessages.emplace_back(
                this->esp.millis(), isAvailable);
        });

        this->mqttClient.setConfig(
            MqttConfig{this->deviceName, {ServerConfig{}}, {"ava", "status"}});
    }

    void loopUntil(unsigned long time, unsigned long delay = 100) {
        this->delayUntil(time, delay, [&]() { this->mqttClient.loop(); });
    }

    void sendStatusMessage(const std::string& mac) {
        DynamicJsonBuffer buffer(200);
        JsonObject& message = buffer.createObject();
        message["name"] = this->deviceName;
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
        BOOST_CHECK_EQUAL_COLLECTIONS(
            this->connectionAttempts.begin(), this->connectionAttempts.end(),
            expectedConnectionAttempts.begin(),
            expectedConnectionAttempts.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(
            this->statusMessages.begin(), this->statusMessages.end(),
            expectedStatusMessages.begin(), expectedStatusMessages.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(
            this->availabilityMessages.begin(),
            this->availabilityMessages.end(),
            expectedAvailabilityMessages.begin(),
            expectedAvailabilityMessages.end());
    }
};

BOOST_FIXTURE_TEST_CASE(NormalFlow, Fixture) {
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

BOOST_FIXTURE_TEST_CASE(RetryConnection, Fixture) {
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

BOOST_FIXTURE_TEST_CASE(
    ConnectionFromSameDevice_Available_StatusFirst, Fixture) {
    this->loopUntil(20, 5);
    this->sendSameDeviceStatus();
    this->loopUntil(40, 5);
    this->sendAvailability(true);
    this->loopUntil(60, 5);

    BOOST_TEST(this->connection.isConnected());
    this->check({{5, true}}, {{30, true, 5, 5.0}}, {{30, true}});
}

BOOST_FIXTURE_TEST_CASE(
    ConnectionFromSameDevice_NotAvailable_StatusFirst, Fixture) {
    this->loopUntil(20, 5);
    this->sendSameDeviceStatus();
    this->loopUntil(40, 5);
    this->sendAvailability(false);
    this->loopUntil(60, 5);

    BOOST_TEST(this->connection.isConnected());
    this->check(
        {{5, true}}, {{30, true, 5, 5.0}, {50, false, 5, 5.0}},
        {{30, true}, {50, true}});
}

BOOST_FIXTURE_TEST_CASE(
    ConnectionFromSameDevice_Available_AvailabilityFirst, Fixture) {
    this->loopUntil(20, 5);
    this->sendAvailability(true);
    this->loopUntil(40, 5);
    this->sendSameDeviceStatus();
    this->loopUntil(60, 5);

    BOOST_TEST(this->connection.isConnected());
    this->check({{5, true}}, {{50, true, 5, 5.0}}, {{50, true}});
}

BOOST_FIXTURE_TEST_CASE(
    ConnectionFromSameDevice_NotAvailable_AvailabilityFirst, Fixture) {
    this->loopUntil(20, 5);
    this->sendAvailability(false);
    this->loopUntil(40, 5);
    this->sendSameDeviceStatus();
    this->loopUntil(60, 5);

    BOOST_TEST(this->connection.isConnected());
    this->check({{5, true}}, {{30, true, 5, 5.0}}, {{30, true}});
}

BOOST_FIXTURE_TEST_CASE(
    ConnectionFromOtherDevice_Available_StatusFirst, Fixture) {
    this->loopUntil(20, 5);
    this->sendOtherDeviceStatus();
    this->loopUntil(40, 5);
    this->sendAvailability(true);
    this->loopUntil(60, 5);

    BOOST_TEST(!this->connection.isConnected());
    this->check({{5, true}}, {}, {{45, false}});
}

BOOST_FIXTURE_TEST_CASE(
    ConnectionFromOtherDevice_NotAvailable_StatusFirst, Fixture) {
    this->loopUntil(20, 5);
    this->sendOtherDeviceStatus();
    this->loopUntil(40, 5);
    this->sendAvailability(false);
    this->loopUntil(60, 5);

    BOOST_TEST(this->connection.isConnected());
    this->check({{5, true}}, {{50, true, 5, 5.0}}, {{50, true}});
}

BOOST_FIXTURE_TEST_CASE(
    ConnectionFromOtherDevice_Available_AvailabilityFirst, Fixture) {
    this->loopUntil(20, 5);
    this->sendAvailability(true);
    this->loopUntil(40, 5);
    this->sendOtherDeviceStatus();
    this->loopUntil(60, 5);

    BOOST_TEST(!this->connection.isConnected());
    this->check({{5, true}}, {}, {{45, false}});
}

BOOST_FIXTURE_TEST_CASE(
    ConnectionFromOtherDevice_NotAvailable_AvailabilityFirst, Fixture) {
    this->loopUntil(20, 5);
    this->sendAvailability(false);
    this->loopUntil(40, 5);
    this->sendOtherDeviceStatus();
    this->loopUntil(60, 5);

    BOOST_TEST(this->connection.isConnected());
    this->check(
        {{5, true}}, {{30, true, 5, 5.0}, {50, false, 5, 5.0}},
        {{30, true}, {50, true}});
}

BOOST_FIXTURE_TEST_CASE(Subscribe, Fixture) {
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
    BOOST_TEST(received == "");

    this->server.publish(
        this->connectionId, FakeMessage{"someTopic", "payload2"});
    this->mqttClient.loop();
    BOOST_TEST(received == "payload2");

    this->server.publish(
        this->connectionId, FakeMessage{"otherTopic", "payload3"});
    this->mqttClient.loop();
    BOOST_TEST(received == "payload2");

    this->server.publish(
        this->connectionId, FakeMessage{"someTopic", "payload4"});
    this->mqttClient.loop();
    BOOST_TEST(received == "payload4");

    this->esp.delay(100000);
    this->mqttClient.loop();
    BOOST_TEST(received == "payload4");
}

BOOST_FIXTURE_TEST_CASE(CycleTime, Fixture) {
    this->sendAvailability(false);
    this->loopUntil(20, 10);
    this->loopUntil(40020, 1000);
    this->loopUntil(60020, 500);

    this->check(
        {{10, true}},
        {
            {20, true, 10, 10.0},
            {60020, false, 1000, 750.0},
        },
        {{20, true}, {60020, true}});
}

BOOST_AUTO_TEST_SUITE_END()
