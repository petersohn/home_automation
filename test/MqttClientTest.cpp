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
    FakeMqttConnection connection{server, [this](bool success) {
        connectionAttempts.emplace_back(esp.millis(), success);
    }};
    DummyBackoff backoff;
    MqttClient mqttClient{debug, esp, wifi, backoff, connection, []() {}};

    size_t connectionId;
    std::vector<StatusMessage> statusMessages;
    std::vector<AvailabilityMessage> availabilityMessages;
    std::vector<ConnectionAttempt> connectionAttempts;
    const std::string deviceName = "this name should be way long enough to fit";

    Fixture() {
        connectionId = server.connect({});
        server.subscribe(
            connectionId, "status", [this](size_t id, FakeMessage message) {
            if (id == connectionId) {
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

            BOOST_TEST(uptime == esp.millis());
            BOOST_TEST(name == deviceName);
            statusMessages.emplace_back(
                uptime, restarted, maxCycleTime, avgCycleTime);
        });
        server.subscribe(
            connectionId, "ava", [this](size_t id, FakeMessage message) {
            if (id == connectionId) {
                return;
            }

            bool isAvailable = false;
            auto res = tools::getBoolValue(
                message.payload.c_str(), isAvailable, message.payload.size());
            BOOST_TEST_REQUIRE(res);
            availabilityMessages.emplace_back(esp.millis(), isAvailable);
        });

        mqttClient.setConfig(
            MqttConfig{deviceName, {ServerConfig{}}, {"ava", "status"}});
    }

    void loopUntil(unsigned long time, unsigned long delay = 100) {
        delayUntil(time, delay, [&]() { mqttClient.loop(); });
    }

    void sendStatusMessage(const std::string& mac) {
        DynamicJsonBuffer buffer(200);
        JsonObject& message = buffer.createObject();
        message["name"] = deviceName;
        message["mac"] = mac;
        std::string result;
        message.printTo(result);
        server.publish(connectionId, {"status", std::move(result), true});
    }

    void sendSameDeviceStatus() { sendStatusMessage(wifi.getMac()); }

    void sendOtherDeviceStatus() { sendStatusMessage("11:11:11:11:11:11"); }

    void sendAvailability(bool value) {
        server.publish(connectionId, {"ava", value ? "1" : "0", true});
    }

    void check(
        const std::vector<ConnectionAttempt>& expectedConnectionAttempts,
        const std::vector<StatusMessage>& expectedStatusMessages,
        const std::vector<AvailabilityMessage>& expectedAvailabilityMessages) {
        BOOST_CHECK_EQUAL_COLLECTIONS(
            connectionAttempts.begin(), connectionAttempts.end(),
            expectedConnectionAttempts.begin(),
            expectedConnectionAttempts.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(
            statusMessages.begin(), statusMessages.end(),
            expectedStatusMessages.begin(), expectedStatusMessages.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(
            availabilityMessages.begin(), availabilityMessages.end(),
            expectedAvailabilityMessages.begin(),
            expectedAvailabilityMessages.end());
    }
};

BOOST_FIXTURE_TEST_CASE(NormalFlow, Fixture) {
    loopUntil(100000);
    connection.disconnect();
    loopUntil(110000);

    check(
        {{100, true}, {100100, true}},
        {
            {2100, true, 100, 100.0},
            {62100, false, 100, 100.0},
            {100200, false, 100, 100.0},
        },
        {{2100, true}, {62100, true}, {100000, false}, {100200, true}});
}

BOOST_FIXTURE_TEST_CASE(RetryConnection, Fixture) {
    server.working = false;
    loopUntil(200000);
    server.working = true;
    loopUntil(250000);

    check(
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
    loopUntil(20, 5);
    sendSameDeviceStatus();
    loopUntil(40, 5);
    sendAvailability(true);
    loopUntil(60, 5);

    BOOST_TEST(connection.isConnected());
    check({{5, true}}, {{30, true, 5, 5.0}}, {{30, true}});
}

BOOST_FIXTURE_TEST_CASE(
    ConnectionFromSameDevice_NotAvailable_StatusFirst, Fixture) {
    loopUntil(20, 5);
    sendSameDeviceStatus();
    loopUntil(40, 5);
    sendAvailability(false);
    loopUntil(60, 5);

    BOOST_TEST(connection.isConnected());
    check(
        {{5, true}}, {{30, true, 5, 5.0}, {50, false, 5, 5.0}},
        {{30, true}, {50, true}});
}

BOOST_FIXTURE_TEST_CASE(
    ConnectionFromSameDevice_Available_AvailabilityFirst, Fixture) {
    loopUntil(20, 5);
    sendAvailability(true);
    loopUntil(40, 5);
    sendSameDeviceStatus();
    loopUntil(60, 5);

    BOOST_TEST(connection.isConnected());
    check({{5, true}}, {{50, true, 5, 5.0}}, {{50, true}});
}

BOOST_FIXTURE_TEST_CASE(
    ConnectionFromSameDevice_NotAvailable_AvailabilityFirst, Fixture) {
    loopUntil(20, 5);
    sendAvailability(false);
    loopUntil(40, 5);
    sendSameDeviceStatus();
    loopUntil(60, 5);

    BOOST_TEST(connection.isConnected());
    check({{5, true}}, {{30, true, 5, 5.0}}, {{30, true}});
}

BOOST_FIXTURE_TEST_CASE(
    ConnectionFromOtherDevice_Available_StatusFirst, Fixture) {
    loopUntil(20, 5);
    sendOtherDeviceStatus();
    loopUntil(40, 5);
    sendAvailability(true);
    loopUntil(60, 5);

    BOOST_TEST(!connection.isConnected());
    check({{5, true}}, {}, {{45, false}});
}

BOOST_FIXTURE_TEST_CASE(
    ConnectionFromOtherDevice_NotAvailable_StatusFirst, Fixture) {
    loopUntil(20, 5);
    sendOtherDeviceStatus();
    loopUntil(40, 5);
    sendAvailability(false);
    loopUntil(60, 5);

    BOOST_TEST(connection.isConnected());
    check({{5, true}}, {{50, true, 5, 5.0}}, {{50, true}});
}

BOOST_FIXTURE_TEST_CASE(
    ConnectionFromOtherDevice_Available_AvailabilityFirst, Fixture) {
    loopUntil(20, 5);
    sendAvailability(true);
    loopUntil(40, 5);
    sendOtherDeviceStatus();
    loopUntil(60, 5);

    BOOST_TEST(!connection.isConnected());
    check({{5, true}}, {}, {{45, false}});
}

BOOST_FIXTURE_TEST_CASE(
    ConnectionFromOtherDevice_NotAvailable_AvailabilityFirst, Fixture) {
    loopUntil(20, 5);
    sendAvailability(false);
    loopUntil(40, 5);
    sendOtherDeviceStatus();
    loopUntil(60, 5);

    BOOST_TEST(connection.isConnected());
    check(
        {{5, true}}, {{30, true, 5, 5.0}, {50, false, 5, 5.0}},
        {{30, true}, {50, true}});
}

BOOST_FIXTURE_TEST_CASE(Subscribe, Fixture) {
    std::string received;

    mqttClient.subscribe(
        "someTopic", [&](const MqttConnection::Message& message) {
        received = std::string{message.payload, message.payloadLength};
    });
    sendAvailability(false);
    mqttClient.loop();
    esp.delay(100);

    server.publish(connectionId, FakeMessage{"otherTopic", "payload1"});
    mqttClient.loop();
    BOOST_TEST(received == "");

    server.publish(connectionId, FakeMessage{"someTopic", "payload2"});
    mqttClient.loop();
    BOOST_TEST(received == "payload2");

    server.publish(connectionId, FakeMessage{"otherTopic", "payload3"});
    mqttClient.loop();
    BOOST_TEST(received == "payload2");

    server.publish(connectionId, FakeMessage{"someTopic", "payload4"});
    mqttClient.loop();
    BOOST_TEST(received == "payload4");

    esp.delay(100000);
    mqttClient.loop();
    BOOST_TEST(received == "payload4");
}

BOOST_FIXTURE_TEST_CASE(CycleTime, Fixture) {
    sendAvailability(false);
    loopUntil(20, 10);
    loopUntil(40020, 1000);
    loopUntil(60020, 500);

    check(
        {{10, true}},
        {
            {20, true, 10, 10.0},
            {60020, false, 1000, 750.0},
        },
        {{20, true}, {60020, true}});
}

BOOST_AUTO_TEST_SUITE_END()
