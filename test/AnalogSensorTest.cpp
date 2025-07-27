#include <boost/test/data/test_case.hpp>
#include <boost/test/tools/old/interface.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <cmath>
#include <memory>
#include <optional>

#include "EspTestBase.hpp"
#include "FakeAnalogInput.hpp"
#include "FakeEspApi.hpp"
#include "common/AnalogInputWithChannel.hpp"
#include "common/AnalogSensor.hpp"
#include "tools/string.hpp"

namespace std {
std::ostream& boost_test_print_type(
    ostream& os, const optional<vector<string>>& value) {
    if (!value) {
        return os << "<none>";
    }

    os << "{";
    for (size_t i = 0; i < value->size(); ++i) {
        os << "\"" << (*value)[i] << "\"";
        if (i < value->size() - 1) {
            os << ", ";
        }
    }
    return os << "}";
}

}  // namespace std

BOOST_AUTO_TEST_SUITE(AnalogSensorTest)

class Fixture : public EspTestBase {
public:
    std::shared_ptr<FakeAnalogInput> input =
        std::make_shared<FakeAnalogInput>();
    std::unique_ptr<AnalogSensor> sensor;

    void init(double max, int precision, unsigned aggregateTime) {
        sensor = std::make_unique<AnalogSensor>(
            debug, esp, AnalogInputWithChannel(input, 0), max, precision,
            aggregateTime);
        esp.delay(10);
    }

    std::optional<std::vector<std::string>> none() { return std::nullopt; }

    std::optional<std::vector<std::string>> expected(std::string value) {
        return std::make_optional<std::vector<std::string>>({std::move(value)});
    }
};

BOOST_FIXTURE_TEST_CASE(Basic, Fixture) {
    init(0.0, 0, 0);
    input->values = {12};
    BOOST_TEST(sensor->measure() == expected("12"));
    input->values = {232};
    BOOST_TEST(sensor->measure() == expected("232"));
}

BOOST_FIXTURE_TEST_CASE(Divide, Fixture) {
    init(16.0, 2, 0);
    input->values = {1024};
    BOOST_TEST(sensor->measure() == expected("16"));
    input->values = {512};
    BOOST_TEST(sensor->measure() == expected("8"));
    input->values = {32};
    BOOST_TEST(sensor->measure() == expected("0.5"));
}

BOOST_FIXTURE_TEST_CASE(AggregateSameValue, Fixture) {
    init(0, 2, 10);

    input->values = {123};
    for (std::size_t i = 0; i < 10; ++i) {
        BOOST_TEST(sensor->measure() == none());
        esp.delay(1);
    }
    BOOST_TEST(sensor->measure() == expected("123"));

    esp.delay(10);

    input->values = {54};
    for (std::size_t i = 0; i < 5; ++i) {
        BOOST_TEST(sensor->measure() == none());
        esp.delay(2);
    }
    BOOST_TEST(sensor->measure() == expected("54"));
}

BOOST_FIXTURE_TEST_CASE(Aggregate50HzSine, Fixture) {
    init(0, 0, 20);

    const double pi = std::acos(-1);
    const double effective = 10000.0;
    const double peak = effective * std::sqrt(2);

    for (std::size_t i = 0; i < 20; ++i) {
        input->values = {static_cast<int>(peak * std::sin(i * pi / 10.0))};
        BOOST_TEST(sensor->measure() == none());
        esp.delay(1);
    }
    auto result = sensor->measure();
    BOOST_REQUIRE(result);
    BOOST_REQUIRE(result->size() == 1);
    auto value = std::stoi((*result)[0]);
    BOOST_TEST(value >= 9900);
    BOOST_TEST(value <= 11000);
}

BOOST_AUTO_TEST_SUITE_END();
