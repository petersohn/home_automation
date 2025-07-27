#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <memory>
#include <optional>

#include "FakeAnalogInput.hpp"
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

class Fixture {
public:
    std::shared_ptr<FakeAnalogInput> input =
        std::make_shared<FakeAnalogInput>();
    std::unique_ptr<AnalogSensor> sensor;

    void init(double max, int precision) {
        sensor = std::make_unique<AnalogSensor>(
            AnalogInputWithChannel(input, 0), max, precision);
    }

    std::optional<std::vector<std::string>> expected(std::string value) {
        return std::make_optional<std::vector<std::string>>({std::move(value)});
    }
};

BOOST_FIXTURE_TEST_CASE(Basic, Fixture) {
    init(0.0, 0);
    input->values = {12};
    BOOST_TEST(sensor->measure() == expected("12"));
    input->values = {232};
    BOOST_TEST(sensor->measure() == expected("232"));
}

BOOST_FIXTURE_TEST_CASE(Divide, Fixture) {
    init(16.0, 2);
    input->values = {1024};
    BOOST_TEST(sensor->measure() == expected("16"));
    input->values = {512};
    BOOST_TEST(sensor->measure() == expected("8"));
    input->values = {32};
    BOOST_TEST(sensor->measure() == expected("0.5"));
}

BOOST_AUTO_TEST_SUITE_END();
