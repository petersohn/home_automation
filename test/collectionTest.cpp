#include "tools/collection.hpp"

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

BOOST_AUTO_TEST_SUITE(CollectionTest)

BOOST_AUTO_TEST_SUITE(FindValueTest)

BOOST_AUTO_TEST_CASE(FindValue) {
    const std::vector<std::pair<std::string, int>> values{
            std::make_pair("foo", 1),
            std::make_pair("bar", 2),
            std::make_pair("foobar", 3),
        };
    BOOST_TEST(tools::findValue(values, "foo") == &values[0].second);
    BOOST_TEST(tools::findValue(values, "bar") == &values[1].second);
    BOOST_TEST(tools::findValue(values, "foobar") == &values[2].second);
}

BOOST_AUTO_TEST_CASE(NotFound) {
    const std::vector<std::pair<std::string, int>> values{
            std::make_pair("foo", 1),
            std::make_pair("bar", 2),
            std::make_pair("foobar", 3),
        };
    BOOST_TEST(tools::findValue(values, "baz") == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
