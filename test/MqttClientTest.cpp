#include "EspTestBase.hpp"
#include "FakeMqttConnection.hpp"
#include "DummyBackoff.hpp"
#include "common/MqttClient.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

BOOST_AUTO_TEST_SUITE(MqttClientTest)

class Fixture : public EspTestBase {
public:
    FakeMqttServer server;
    FakeMqttConnection connection{server};
    DummyBackoff backoff;
    MqttClient mqttClient{debug, esp, wifi, backoff, connection, []() {}};
};

BOOST_AUTO_TEST_SUITE_END()
