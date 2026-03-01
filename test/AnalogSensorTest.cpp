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

    void init(
        double max, double offset, int precision, unsigned aggregateTime,
        double cutoff) {
        this->sensor = std::make_unique<AnalogSensor>(
            this->esp, this->debug, AnalogInputWithChannel(this->input, 0), max,
            offset, cutoff, precision, aggregateTime);
        this->esp.delay(10);
    }

    std::optional<std::vector<std::string>> none() { return std::nullopt; }

    std::optional<std::vector<std::string>> expected(std::string value) {
        return std::make_optional<std::vector<std::string>>({std::move(value)});
    }

    std::optional<std::vector<std::string>> expected2(
        std::string avg, std::string max) {
        return std::make_optional<std::vector<std::string>>(
            {std::move(avg), std::move(max)});
    }
};

BOOST_FIXTURE_TEST_CASE(Basic, Fixture) {
    this->init(0.0, 0.0, 0, 0, 0.0);
    this->input->values = {12};
    BOOST_TEST(this->sensor->measure() == this->expected("12"));
    this->input->values = {232};
    BOOST_TEST(this->sensor->measure() == this->expected("232"));
}

BOOST_FIXTURE_TEST_CASE(Divide, Fixture) {
    this->init(16.0, 0.0, 2, 0, 0.0);
    this->input->values = {1024};
    BOOST_TEST(this->sensor->measure() == this->expected("16"));
    this->input->values = {512};
    BOOST_TEST(this->sensor->measure() == this->expected("8"));
    this->input->values = {0};
    BOOST_TEST(this->sensor->measure() == this->expected("0"));
    this->input->values = {32};
    BOOST_TEST(this->sensor->measure() == this->expected("0.5"));
}

BOOST_FIXTURE_TEST_CASE(Offset, Fixture) {
    this->init(16.0, 8.0, 2, 0, 0.0);
    this->input->values = {1024};
    BOOST_TEST(this->sensor->measure() == this->expected("8"));
    this->input->values = {512};
    BOOST_TEST(this->sensor->measure() == this->expected("0"));
    this->input->values = {0};
    BOOST_TEST(this->sensor->measure() == this->expected("-8"));
    this->input->values = {544};
    BOOST_TEST(this->sensor->measure() == this->expected("0.5"));
    this->input->values = {480};
    BOOST_TEST(this->sensor->measure() == this->expected("-0.5"));
}

BOOST_FIXTURE_TEST_CASE(Cutoff, Fixture) {
    this->init(16.0, 8.0, 2, 0, 1.0);
    this->input->values = {1024};
    BOOST_TEST(this->sensor->measure() == this->expected("8"));
    this->input->values = {512};
    BOOST_TEST(this->sensor->measure() == this->expected("0"));
    this->input->values = {0};
    BOOST_TEST(this->sensor->measure() == this->expected("-8"));
    this->input->values = {449};
    BOOST_TEST(this->sensor->measure() == this->expected("0"));
    this->input->values = {448};
    BOOST_TEST(this->sensor->measure() == this->expected("-1"));
    this->input->values = {575};
    BOOST_TEST(this->sensor->measure() == this->expected("0"));
    this->input->values = {576};
    BOOST_TEST(this->sensor->measure() == this->expected("1"));
}

BOOST_FIXTURE_TEST_CASE(AggregateSameValue, Fixture) {
    this->init(0, 0, 2, 10, 0.0);

    this->input->values = {123};
    for (std::size_t i = 0; i < 10; ++i) {
        BOOST_TEST(this->sensor->measure() == this->none());
        this->esp.delay(1);
    }
    BOOST_TEST(this->sensor->measure() == this->expected2("123", "123"));

    this->esp.delay(10);

    this->input->values = {54};
    for (std::size_t i = 0; i < 5; ++i) {
        BOOST_TEST(this->sensor->measure() == this->none());
        this->esp.delay(2);
    }
    BOOST_TEST(this->sensor->measure() == this->expected2("54", "54"));
}

namespace {
const auto delays = boost::unit_test::data::make({1, 2});
}

BOOST_DATA_TEST_CASE_F(Fixture, Aggregate50HzSine, delays, delay) {
    this->init(0, 0, 0, 20, 0.0);

    const double pi = std::acos(-1);
    const double effective = 10000.0;
    const double peak = effective * std::sqrt(2);

    for (std::size_t i = 0; i < 20; i += delay) {
        this->input->values = {
            std::abs(static_cast<int>(peak * std::sin(i * pi / 10.0)))};
        BOOST_TEST(this->sensor->measure() == this->none());
        this->esp.delay(delay);
    }
    auto result = this->sensor->measure();
    BOOST_REQUIRE(result);
    BOOST_REQUIRE(result->size() == 2);
    auto avg = std::stoi((*result)[0]);
    auto max = std::stoi((*result)[1]);
    BOOST_TEST_MESSAGE("avg=" << avg << " max=" << max);
    BOOST_TEST(avg >= effective * 0.97);
    BOOST_TEST(avg <= effective * 1.03);
    BOOST_TEST(max >= peak * 0.95);
    BOOST_TEST(max <= peak);
}

BOOST_DATA_TEST_CASE_F(Fixture, Aggregate50HzSineWithScaling, delays, delay) {
    const double peakOut = 10000.0;
    const double realPeakOut = peakOut / 2.0;
    const double peakIn = peakOut / 100;
    const double effective = realPeakOut / std::sqrt(2);
    const double pi = std::acos(-1);

    this->init(peakOut, peakOut / 2.0, 0, 20, 0.0);

    this->input->maxValue = peakIn;

    for (std::size_t i = 0; i < 20; i += delay) {
        this->input->values = {std::abs(
            static_cast<int>(peakIn / 2.0 * (std::sin(i * pi / 10.0) + 1)))};
        BOOST_TEST(this->sensor->measure() == this->none());
        this->esp.delay(delay);
    }
    auto result = this->sensor->measure();
    BOOST_REQUIRE(result);
    BOOST_REQUIRE(result->size() == 2);
    auto avg = std::stoi((*result)[0]);
    auto max = std::stoi((*result)[1]);
    BOOST_TEST_MESSAGE("avg=" << avg << " max=" << max);
    BOOST_TEST(avg >= effective * 0.97);
    BOOST_TEST(avg <= effective * 1.03);
    BOOST_TEST(max >= realPeakOut * 0.95);
    BOOST_TEST(max <= realPeakOut);
}
BOOST_AUTO_TEST_SUITE_END();
