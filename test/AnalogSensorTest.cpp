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
void PrintTo(
    const std::optional<std::vector<std::string>>& value, std::ostream* os) {
    if (!value) {
        *os << "<none>";
        return;
    }
    *os << "{";
    for (size_t i = 0; i < value->size(); ++i) {
        *os << "\"" << (*value)[i] << "\"";
        if (i + 1 < value->size()) {
            *os << ", ";
        }
    }
    *os << "}";
}
}  // namespace std

class AnalogSensorTest : public EspTestBase,
                         public ::testing::WithParamInterface<int> {
public:
    std::shared_ptr<FakeAnalogInput> input =
        std::make_shared<FakeAnalogInput>();
    std::unique_ptr<AnalogSensor> sensor;

    void init(
        double max, double offset, int precision, unsigned aggregateTime,
        double cutoff) {
        this->sensor = std::make_unique<AnalogSensor>(
            this->esp, this->debug, AnalogInputWithChannel(this->input, 0), max,
            offset, cutoff, precision, aggregateTime, 0 /*aggregateDelay*/);
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

TEST_F(AnalogSensorTest, Basic) {
    this->init(0.0, 0.0, 0, 0, 0.0);
    this->input->values = {12};
    EXPECT_EQ(this->sensor->measure(), this->expected("12"));
    this->input->values = {232};
    EXPECT_EQ(this->sensor->measure(), this->expected("232"));
}

TEST_F(AnalogSensorTest, Divide) {
    this->init(16.0, 0.0, 2, 0, 0.0);
    this->input->values = {1024};
    EXPECT_EQ(this->sensor->measure(), this->expected("16"));
    this->input->values = {512};
    EXPECT_EQ(this->sensor->measure(), this->expected("8"));
    this->input->values = {0};
    EXPECT_EQ(this->sensor->measure(), this->expected("0"));
    this->input->values = {32};
    EXPECT_EQ(this->sensor->measure(), this->expected("0.5"));
}

TEST_F(AnalogSensorTest, Offset) {
    this->init(16.0, 8.0, 2, 0, 0.0);
    this->input->values = {1024};
    EXPECT_EQ(this->sensor->measure(), this->expected("8"));
    this->input->values = {512};
    EXPECT_EQ(this->sensor->measure(), this->expected("0"));
    this->input->values = {0};
    EXPECT_EQ(this->sensor->measure(), this->expected("-8"));
    this->input->values = {544};
    EXPECT_EQ(this->sensor->measure(), this->expected("0.5"));
    this->input->values = {480};
    EXPECT_EQ(this->sensor->measure(), this->expected("-0.5"));
}

TEST_F(AnalogSensorTest, Cutoff) {
    this->init(16.0, 8.0, 2, 0, 1.0);
    this->input->values = {1024};
    EXPECT_EQ(this->sensor->measure(), this->expected("8"));
    this->input->values = {512};
    EXPECT_EQ(this->sensor->measure(), this->expected("0"));
    this->input->values = {0};
    EXPECT_EQ(this->sensor->measure(), this->expected("-8"));
    this->input->values = {449};
    EXPECT_EQ(this->sensor->measure(), this->expected("0"));
    this->input->values = {448};
    EXPECT_EQ(this->sensor->measure(), this->expected("-1"));
    this->input->values = {575};
    EXPECT_EQ(this->sensor->measure(), this->expected("0"));
    this->input->values = {576};
    EXPECT_EQ(this->sensor->measure(), this->expected("1"));
}

TEST_F(AnalogSensorTest, AggregateSameValue) {
    this->init(0, 0, 2, 10, 0.0);

    this->input->values = {123};
    for (std::size_t i = 0; i < 10; ++i) {
        EXPECT_EQ(this->sensor->measure(), this->none());
        this->esp.delay(1);
    }
    EXPECT_EQ(this->sensor->measure(), this->expected2("123", "123"));

    this->esp.delay(10);

    this->input->values = {54};
    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(this->sensor->measure(), this->none());
        this->esp.delay(2);
    }
    EXPECT_EQ(this->sensor->measure(), this->expected2("54", "54"));
}

TEST_P(AnalogSensorTest, Aggregate50HzSine) {
    this->init(0, 0, 0, 20, 0.0);

    const double pi = std::acos(-1);
    const double effective = 10000.0;
    const double peak = effective * std::sqrt(2);

    for (std::size_t i = 0; i < 20; i += this->GetParam()) {
        this->input->values = {
            std::abs(static_cast<int>(peak * std::sin(i * pi / 10.0)))};
        EXPECT_EQ(this->sensor->measure(), this->none());
        this->esp.delay(this->GetParam());
    }
    auto result = this->sensor->measure();
    ASSERT_TRUE(result);
    ASSERT_EQ(result->size(), 2u);
    auto avg = std::stoi((*result)[0]);
    auto max = std::stoi((*result)[1]);
    RecordProperty("avg", avg);
    RecordProperty("max", max);
    EXPECT_GE(avg, static_cast<int>(effective * 0.97));
    EXPECT_LE(avg, static_cast<int>(effective * 1.03));
    EXPECT_GE(max, static_cast<int>(peak * 0.95));
    EXPECT_LE(max, static_cast<int>(peak));
}

TEST_P(AnalogSensorTest, Aggregate50HzSineWithScaling) {
    const double peakOut = 10000.0;
    const double realPeakOut = peakOut / 2.0;
    const double peakIn = peakOut / 100;
    const double effective = realPeakOut / std::sqrt(2);
    const double pi = std::acos(-1);

    this->init(peakOut, peakOut / 2.0, 0, 20, 0.0);

    this->input->maxValue = peakIn;

    for (std::size_t i = 0; i < 20; i += this->GetParam()) {
        this->input->values = {std::abs(
            static_cast<int>(peakIn / 2.0 * (std::sin(i * pi / 10.0) + 1)))};
        EXPECT_EQ(this->sensor->measure(), this->none());
        this->esp.delay(this->GetParam());
    }
    auto result = this->sensor->measure();
    ASSERT_TRUE(result);
    ASSERT_EQ(result->size(), 2u);
    auto avg = std::stoi((*result)[0]);
    auto max = std::stoi((*result)[1]);
    RecordProperty("avg", avg);
    RecordProperty("max", max);
    EXPECT_GE(avg, static_cast<int>(effective * 0.97));
    EXPECT_LE(avg, static_cast<int>(effective * 1.03));
    EXPECT_GE(max, static_cast<int>(realPeakOut * 0.95));
    EXPECT_LE(max, static_cast<int>(realPeakOut));
}

INSTANTIATE_TEST_SUITE_P(
    AnalogSensorTest, AnalogSensorTest, testing::Values(1, 2),
    [](const testing::TestParamInfo<AnalogSensorTest::ParamType>& info) {
        return "delay" + std::to_string(info.param);
    });
