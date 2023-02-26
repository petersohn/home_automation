#include "EspTestBase.hpp"
#include "FakeMqttConnection.hpp"
#include "DummyBackoff.hpp"
#include "common/MqttClient.hpp"
#include "common/ArduinoJson.hpp"
#include "tools/string.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

using namespace ArduinoJson;

BOOST_AUTO_TEST_SUITE(MqttClientTest)

struct StatusMessage {
    unsigned long time;
    bool restarted;

    StatusMessage(unsigned long time, bool restarted)
        : time(time), restarted(restarted) {}
};

bool operator==(const StatusMessage& lhs, const StatusMessage& rhs) {
    return std::tie(lhs.time, lhs.restarted) == std::tie(rhs.time, rhs.restarted);
}

bool operator!=(const StatusMessage& lhs, const StatusMessage& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const StatusMessage& msg) {
    os << msg.time;
    if (msg.restarted) {
        os << " restarted";
    }
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

bool operator==(const AvailabilityMessage& lhs, const AvailabilityMessage& rhs) {
    return std::tie(lhs.time, lhs.isAvailable) ==
        std::tie(rhs.time,rhs.isAvailable);
}

bool operator!=(const AvailabilityMessage& lhs, const AvailabilityMessage& rhs) {
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
    return std::tie(lhs.time, lhs.success) ==
        std::tie(rhs.time,rhs.success);
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

    Fixture() {
        connectionId = server.connect({});
        server.subscribe(connectionId, "status", [this](
                    size_t id, MqttConnection::Message message) {
                if (id == connectionId) {
                    return;
                }

                DynamicJsonBuffer buffer(200);
                auto& json = buffer.parseObject(message.payload);
                auto name = json.get<std::string>("name");
                auto uptime = json.get<unsigned long>("uptime");
                auto restarted = json.get<bool>("restarted");

                BOOST_TEST(uptime == esp.millis());
                BOOST_TEST(name == "test");
                statusMessages.emplace_back(uptime, restarted);
            });
        server.subscribe(connectionId, "ava", [this](
                    size_t id, MqttConnection::Message message) {
                if (id == connectionId) {
                    return;
                }

                bool isAvailable = false;
                auto res = tools::getBoolValue(message.payload, isAvailable);
                BOOST_TEST_REQUIRE(res);
                availabilityMessages.emplace_back(esp.millis(), isAvailable);
            });

        mqttClient.setConfig(
            MqttConfig{"test", {ServerConfig{}}, {"ava", "status"}});
    }

    void loopUntil(unsigned long time, unsigned long delay = 100) {
        delayUntil(time, delay, [&]() { mqttClient.loop(); });
    }

    void sendStatusMessage(const std::string& mac) {
        DynamicJsonBuffer buffer(200);
        JsonObject& message = buffer.createObject();
        message["name"] = "test";
        message["mac"] = mac;
        std::string result;
        message.printTo(result);
        server.publish(connectionId, {"status", std::move(result), true});
    }

    void sendSameDeviceStatus() {
        sendStatusMessage(wifi.getMac());
    }

    void sendOtherDeviceStatus() {
        sendStatusMessage("11:11:11:11:11:11");
    }

    void sendAvailability(bool value) {
        server.publish(connectionId, {"ava", value ? "1" : "0", true});
    }

    void check(
            const std::vector<ConnectionAttempt>& expectedConnectionAttempts,
            const std::vector<StatusMessage>& expectedStatusMessages,
            const std::vector<AvailabilityMessage>& expectedAvailabilityMessages) {
        BOOST_CHECK_EQUAL_COLLECTIONS(
                connectionAttempts.begin(),
                connectionAttempts.end(),
                expectedConnectionAttempts.begin(),
                expectedConnectionAttempts.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(
                statusMessages.begin(),
                statusMessages.end(),
                expectedStatusMessages.begin(),
                expectedStatusMessages.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(
                availabilityMessages.begin(),
                availabilityMessages.end(),
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
        {{2100, true}, {62100, false}, {100200, false}},
        {{2100, true}, {62100, true}, {100000, false}, {100200, true}}
        );
}

BOOST_FIXTURE_TEST_CASE(RetryConnection, Fixture) {
    server.working = false;
    loopUntil(200000);
    server.working = true;
    loopUntil(250000);

    check(
        {{100, false}, {600, false}, {1600, false}, {3600, false},
         {7600, false}, {15600, false}, {31600, false}, {63600, false},
         {123600, false}, {183600, false}, {243600, true}},
        {{245600, true}},
        {{245600, true}});
}

BOOST_FIXTURE_TEST_CASE(
        ConnectionFromSameDevice_Available_StatusFirst, Fixture) {
    loopUntil(20, 5);
    sendSameDeviceStatus();
    loopUntil(40, 5);
    sendAvailability(true);
    loopUntil(60, 5);

    BOOST_TEST(connection.isConnected());
    check(
        {{5, true}},
        {{30, true}},
        {{30, true}});
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
        {{5, true}},
        {{30, true}, {50, false}},
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
    check(
        {{5, true}},
        {{50, true}},
        {{50, true}});
}

BOOST_FIXTURE_TEST_CASE(
        ConnectionFromSameDevice_NotAvailable_AvailabilityFirst, Fixture) {
    loopUntil(20, 5);
    sendAvailability(false);
    loopUntil(40, 5);
    sendSameDeviceStatus();
    loopUntil(60, 5);

    BOOST_TEST(connection.isConnected());
    check(
        {{5, true}},
        {{30, true}},
        {{30, true}});
}

BOOST_FIXTURE_TEST_CASE(
        ConnectionFromOtherDevice_Available_StatusFirst, Fixture) {
    loopUntil(20, 5);
    sendOtherDeviceStatus();
    loopUntil(40, 5);
    sendAvailability(true);
    loopUntil(60, 5);

    BOOST_TEST(!connection.isConnected());
    check(
        {{5, true}},
        {},
        {{45, false}});
}

BOOST_FIXTURE_TEST_CASE(
        ConnectionFromOtherDevice_NotAvailable_StatusFirst, Fixture) {
    loopUntil(20, 5);
    sendOtherDeviceStatus();
    loopUntil(40, 5);
    sendAvailability(false);
    loopUntil(60, 5);

    BOOST_TEST(connection.isConnected());
    check(
        {{5, true}},
        {{50, true}},
        {{50, true}});
}

BOOST_FIXTURE_TEST_CASE(
        ConnectionFromOtherDevice_Available_AvailabilityFirst, Fixture) {
    loopUntil(20, 5);
    sendAvailability(true);
    loopUntil(40, 5);
    sendOtherDeviceStatus();
    loopUntil(60, 5);

    BOOST_TEST(!connection.isConnected());
    check(
        {{5, true}},
        {},
        {{45, false}});
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
        {{5, true}},
        {{30, true}, {50, false}},
        {{30, true}, {50, true}});
}

BOOST_AUTO_TEST_SUITE_END()
