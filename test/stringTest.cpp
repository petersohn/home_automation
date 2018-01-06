#ifndef TEST_STRINGTEST_CPP
#define TEST_STRINGTEST_CPP

#include "tools/string.hpp"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(StringTest)

BOOST_AUTO_TEST_SUITE(NextTokenTest)

BOOST_AUTO_TEST_CASE(ReadTokensInString) {
    const std::string s = "foo bar foobar baz";
    std::size_t position = 0;

    BOOST_TEST(tools::nextToken(s, ' ', position) == "foo");
    BOOST_TEST(tools::nextToken(s, ' ', position) == "bar");
    BOOST_TEST(tools::nextToken(s, ' ', position) == "foobar");
    BOOST_TEST(tools::nextToken(s, ' ', position) == "baz");
    BOOST_TEST(tools::nextToken(s, ' ', position) == "");
    BOOST_TEST(position == s.size());
}

BOOST_AUTO_TEST_CASE(MultipleSeparatorsAtBeginning) {
    const std::string s = "   foo bar";
    std::size_t position = 0;

    BOOST_TEST(tools::nextToken(s, ' ', position) == "foo");
    BOOST_TEST(tools::nextToken(s, ' ', position) == "bar");
    BOOST_TEST(tools::nextToken(s, ' ', position) == "");
    BOOST_TEST(position == s.size());
}

BOOST_AUTO_TEST_CASE(MultipleSeparatorsAtEnd) {
    const std::string s = "foobar foo   ";
    std::size_t position = 0;

    BOOST_TEST(tools::nextToken(s, ' ', position) == "foobar");
    BOOST_TEST(tools::nextToken(s, ' ', position) == "foo");
    BOOST_TEST(tools::nextToken(s, ' ', position) == "");
    BOOST_TEST(position == s.size());
}

BOOST_AUTO_TEST_CASE(MultipleSeparatorsBetweenElements) {
    const std::string s = "foobar  foo   baz barbar";
    std::size_t position = 0;

    BOOST_TEST(tools::nextToken(s, ' ', position) == "foobar");
    BOOST_TEST(tools::nextToken(s, ' ', position) == "foo");
    BOOST_TEST(tools::nextToken(s, ' ', position) == "baz");
    BOOST_TEST(tools::nextToken(s, ' ', position) == "barbar");
    BOOST_TEST(tools::nextToken(s, ' ', position) == "");
    BOOST_TEST(position == s.size());
}

BOOST_AUTO_TEST_CASE(DifferentSeparator) {
    const std::string s = "foo#bar#foobar";
    std::size_t position = 0;

    BOOST_TEST(tools::nextToken(s, '#', position) == "foo");
    BOOST_TEST(tools::nextToken(s, '#', position) == "bar");
    BOOST_TEST(tools::nextToken(s, '#', position) == "foobar");
    BOOST_TEST(tools::nextToken(s, '#', position) == "");
    BOOST_TEST(position == s.size());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(JoinTest)

BOOST_AUTO_TEST_CASE(SpaceAsSeparator) {
    tools::Join join{" "};
    join.add("foo");
    join.add("bar");
    join.add("foobar");
    join.add("baz");
    BOOST_TEST(join.get() == "foo bar foobar baz");
}

BOOST_AUTO_TEST_CASE(LongerSeparator) {
    tools::Join join{"-->"};
    join.add("foo");
    join.add("barfoo");
    join.add("baz");
    BOOST_TEST(join.get() == "foo-->barfoo-->baz");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(SubstituteTest)

BOOST_AUTO_TEST_CASE(SingleValue) {
    const std::vector<std::string> values{"baz"};
    BOOST_TEST(tools::substitute("foo%1bar", values) == "foobazbar");
}

BOOST_AUTO_TEST_CASE(MultipleValues) {
    const std::vector<std::string> values{"foo", "foobar"};
    BOOST_TEST(tools::substitute("%1 %2", values) == "foo foobar");
}

BOOST_AUTO_TEST_CASE(DifferentOrder) {
    const std::vector<std::string> values{"foo", "bar", "baz"};
    BOOST_TEST(tools::substitute("%3+%1-%2", values) == "baz+foo-bar");
}

BOOST_AUTO_TEST_CASE(MultipleOccurrances) {
    const std::vector<std::string> values{"foo", "bar"};
    BOOST_TEST(tools::substitute("%1%1%2%2%1", values) == "foofoobarbarfoo");
}

BOOST_AUTO_TEST_CASE(IgnoreInvalidReference) {
    const std::vector<std::string> values{"foo", "bar"};
    BOOST_TEST(tools::substitute("%1 %3 %2", values) == "foo  bar");
}

BOOST_AUTO_TEST_CASE(LongerSequence) {
    std::vector<std::string> values;
    for (int i = 0; i < 1000; ++i) {
        values.push_back("value" + std::to_string(i));
    }
    BOOST_TEST(tools::substitute("%124-->%654", values) ==
            "value123-->value653");
}

BOOST_AUTO_TEST_CASE(LiteralPercent) {
    const std::vector<std::string> values{"foo", "bar"};
    BOOST_TEST(tools::substitute("%% %1%%%2%%", values) == "% foo%bar%");
}

BOOST_AUTO_TEST_CASE(IgnoreStrayPercent) {
    const std::vector<std::string> values{"foo", "bar"};
    BOOST_TEST(tools::substitute("foo%bar", values) == "foobar");
}

BOOST_AUTO_TEST_CASE(IgnoreZeroReference) {
    const std::vector<std::string> values{"foo", "bar"};
    BOOST_TEST(tools::substitute("%0 %1 %2", values) == " foo bar");
}

BOOST_AUTO_TEST_CASE(IgnoreNegativeReference) {
    const std::vector<std::string> values{"foo", "bar"};
    BOOST_TEST(tools::substitute("%-1", values) == "-1");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(GetBoolValueTest)

BOOST_AUTO_TEST_CASE(Zero) {
    bool result = false;
    BOOST_TEST(tools::getBoolValue("0", result));
    BOOST_TEST(result == false);
}

BOOST_AUTO_TEST_CASE(One) {
    bool result = false;
    BOOST_TEST(tools::getBoolValue("1", result));
    BOOST_TEST(result == true);
}

BOOST_AUTO_TEST_CASE(Off) {
    bool result = false;
    BOOST_TEST(tools::getBoolValue("off", result));
    BOOST_TEST(result == false);
    BOOST_TEST(tools::getBoolValue("Off", result));
    BOOST_TEST(result == false);
    BOOST_TEST(tools::getBoolValue("OFF", result));
    BOOST_TEST(result == false);
    BOOST_TEST(tools::getBoolValue("oFf", result));
    BOOST_TEST(result == false);
}

BOOST_AUTO_TEST_CASE(On) {
    bool result = false;
    BOOST_TEST(tools::getBoolValue("on", result));
    BOOST_TEST(result == true);
    BOOST_TEST(tools::getBoolValue("On", result));
    BOOST_TEST(result == true);
    BOOST_TEST(tools::getBoolValue("oN", result));
    BOOST_TEST(result == true);
    BOOST_TEST(tools::getBoolValue("ON", result));
    BOOST_TEST(result == true);
}

BOOST_AUTO_TEST_CASE(False) {
    bool result = false;
    BOOST_TEST(tools::getBoolValue("false", result));
    BOOST_TEST(result == false);
    BOOST_TEST(tools::getBoolValue("False", result));
    BOOST_TEST(result == false);
    BOOST_TEST(tools::getBoolValue("fAlSe", result));
    BOOST_TEST(result == false);
    BOOST_TEST(tools::getBoolValue("FALSE", result));
    BOOST_TEST(result == false);
}

BOOST_AUTO_TEST_CASE(True) {
    bool result = false;
    BOOST_TEST(tools::getBoolValue("true", result));
    BOOST_TEST(result == true);
    BOOST_TEST(tools::getBoolValue("True", result));
    BOOST_TEST(result == true);
    BOOST_TEST(tools::getBoolValue("tRuE", result));
    BOOST_TEST(result == true);
    BOOST_TEST(tools::getBoolValue("TRUE", result));
    BOOST_TEST(result == true);
}

BOOST_AUTO_TEST_CASE(DifferentStartingValue) {
    bool result = true;
    BOOST_TEST(tools::getBoolValue("1", result));
    BOOST_TEST(result == true);
    BOOST_TEST(tools::getBoolValue("0", result));
    BOOST_TEST(result == false);
}

BOOST_AUTO_TEST_CASE(InvalidValue) {
    bool result = false;
    BOOST_TEST(!tools::getBoolValue("foo", result));
    BOOST_TEST(result == false);
    result = true;
    BOOST_TEST(!tools::getBoolValue("foo", result));
    BOOST_TEST(result == true);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

#endif // TEST_STRINGTEST_CPP
