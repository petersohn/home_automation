#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "tools/collection.hpp"

TEST(CollectionFindValueTest, FindValue) {
    const std::vector<std::pair<std::string, int>> values{
        std::make_pair("foo", 1),
        std::make_pair("bar", 2),
        std::make_pair("foobar", 3),
    };
    EXPECT_EQ(tools::findValue(values, "foo"), &values[0].second);
    EXPECT_EQ(tools::findValue(values, "bar"), &values[1].second);
    EXPECT_EQ(tools::findValue(values, "foobar"), &values[2].second);
}

TEST(CollectionFindValueTest, NotFound) {
    const std::vector<std::pair<std::string, int>> values{
        std::make_pair("foo", 1),
        std::make_pair("bar", 2),
        std::make_pair("foobar", 3),
    };
    EXPECT_EQ(tools::findValue(values, "baz"), nullptr);
}
