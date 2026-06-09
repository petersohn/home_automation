#include <gtest/gtest.h>

#include "tools/string.hpp"

TEST(StringTest, NextTokenTest_ReadTokensInString) {
    const std::string s = "foo bar foobar baz";
    std::size_t position = 0;

    EXPECT_EQ(tools::nextToken(s, ' ', position), "foo");
    EXPECT_EQ(tools::nextToken(s, ' ', position), "bar");
    EXPECT_EQ(tools::nextToken(s, ' ', position), "foobar");
    EXPECT_EQ(tools::nextToken(s, ' ', position), "baz");
    EXPECT_EQ(tools::nextToken(s, ' ', position), "");
    EXPECT_EQ(position, s.size());
}

TEST(StringTest, NextTokenTest_MultipleSeparatorsAtBeginning) {
    const std::string s = "   foo bar";
    std::size_t position = 0;

    EXPECT_EQ(tools::nextToken(s, ' ', position), "foo");
    EXPECT_EQ(tools::nextToken(s, ' ', position), "bar");
    EXPECT_EQ(tools::nextToken(s, ' ', position), "");
    EXPECT_EQ(position, s.size());
}

TEST(StringTest, NextTokenTest_MultipleSeparatorsAtEnd) {
    const std::string s = "foobar foo   ";
    std::size_t position = 0;

    EXPECT_EQ(tools::nextToken(s, ' ', position), "foobar");
    EXPECT_EQ(tools::nextToken(s, ' ', position), "foo");
    EXPECT_EQ(tools::nextToken(s, ' ', position), "");
    EXPECT_EQ(position, s.size());
}

TEST(StringTest, NextTokenTest_MultipleSeparatorsBetweenElements) {
    const std::string s = "foobar  foo   baz barbar";
    std::size_t position = 0;

    EXPECT_EQ(tools::nextToken(s, ' ', position), "foobar");
    EXPECT_EQ(tools::nextToken(s, ' ', position), "foo");
    EXPECT_EQ(tools::nextToken(s, ' ', position), "baz");
    EXPECT_EQ(tools::nextToken(s, ' ', position), "barbar");
    EXPECT_EQ(tools::nextToken(s, ' ', position), "");
    EXPECT_EQ(position, s.size());
}

TEST(StringTest, NextTokenTest_DifferentSeparator) {
    const std::string s = "foo#bar#foobar";
    std::size_t position = 0;

    EXPECT_EQ(tools::nextToken(s, '#', position), "foo");
    EXPECT_EQ(tools::nextToken(s, '#', position), "bar");
    EXPECT_EQ(tools::nextToken(s, '#', position), "foobar");
    EXPECT_EQ(tools::nextToken(s, '#', position), "");
    EXPECT_EQ(position, s.size());
}

TEST(StringTest, JoinTest_SpaceAsSeparator) {
    tools::Join join{" "};
    join.add("foo");
    join.add("bar");
    join.add("foobar");
    join.add("baz");
    EXPECT_EQ(join.get(), "foo bar foobar baz");
}

TEST(StringTest, JoinTest_LongerSeparator) {
    tools::Join join{"-->"};
    join.add("foo");
    join.add("barfoo");
    join.add("baz");
    EXPECT_EQ(join.get(), "foo-->barfoo-->baz");
}

TEST(StringTest, SubstituteTest_SingleValue) {
    const std::vector<std::string> values{"baz"};
    EXPECT_EQ(tools::substitute("foo%1bar", values), "foobazbar");
}

TEST(StringTest, SubstituteTest_MultipleValues) {
    const std::vector<std::string> values{"foo", "foobar"};
    EXPECT_EQ(tools::substitute("%1 %2", values), "foo foobar");
}

TEST(StringTest, SubstituteTest_DifferentOrder) {
    const std::vector<std::string> values{"foo", "bar", "baz"};
    EXPECT_EQ(tools::substitute("%3+%1-%2", values), "baz+foo-bar");
}

TEST(StringTest, SubstituteTest_MultipleOccurrances) {
    const std::vector<std::string> values{"foo", "bar"};
    EXPECT_EQ(tools::substitute("%1%1%2%2%1", values), "foofoobarbarfoo");
}

TEST(StringTest, SubstituteTest_IgnoreInvalidReference) {
    const std::vector<std::string> values{"foo", "bar"};
    EXPECT_EQ(tools::substitute("%1 %3 %2", values), "foo  bar");
}

TEST(StringTest, SubstituteTest_LongerSequence) {
    std::vector<std::string> values;
    for (int i = 0; i < 1000; ++i) {
        values.push_back("value" + std::to_string(i));
    }
    EXPECT_EQ(tools::substitute("%124-->%654", values), "value123-->value653");
}

TEST(StringTest, SubstituteTest_LiteralPercent) {
    const std::vector<std::string> values{"foo", "bar"};
    EXPECT_EQ(tools::substitute("%% %1%%%2%%", values), "% foo%bar%");
}

TEST(StringTest, SubstituteTest_IgnoreStrayPercent) {
    const std::vector<std::string> values{"foo", "bar"};
    EXPECT_EQ(tools::substitute("foo%bar", values), "foobar");
}

TEST(StringTest, SubstituteTest_IgnoreZeroReference) {
    const std::vector<std::string> values{"foo", "bar"};
    EXPECT_EQ(tools::substitute("%0 %1 %2", values), " foo bar");
}

TEST(StringTest, SubstituteTest_IgnoreNegativeReference) {
    const std::vector<std::string> values{"foo", "bar"};
    EXPECT_EQ(tools::substitute("%-1", values), "-1");
}

TEST(StringTest, GetBoolValueTest_Zero) {
    bool result = false;
    EXPECT_TRUE(tools::getBoolValue("0", result));
    EXPECT_FALSE(result);
}

TEST(StringTest, GetBoolValueTest_One) {
    bool result = false;
    EXPECT_TRUE(tools::getBoolValue("1", result));
    EXPECT_TRUE(result);
}

TEST(StringTest, GetBoolValueTest_Off) {
    bool result = false;
    EXPECT_TRUE(tools::getBoolValue("off", result));
    EXPECT_FALSE(result);
    EXPECT_TRUE(tools::getBoolValue("Off", result));
    EXPECT_FALSE(result);
    EXPECT_TRUE(tools::getBoolValue("OFF", result));
    EXPECT_FALSE(result);
    EXPECT_TRUE(tools::getBoolValue("oFf", result));
    EXPECT_FALSE(result);
}

TEST(StringTest, GetBoolValueTest_On) {
    bool result = false;
    EXPECT_TRUE(tools::getBoolValue("on", result));
    EXPECT_TRUE(result);
    EXPECT_TRUE(tools::getBoolValue("On", result));
    EXPECT_TRUE(result);
    EXPECT_TRUE(tools::getBoolValue("oN", result));
    EXPECT_TRUE(result);
    EXPECT_TRUE(tools::getBoolValue("ON", result));
    EXPECT_TRUE(result);
}

TEST(StringTest, GetBoolValueTest_False) {
    bool result = false;
    EXPECT_TRUE(tools::getBoolValue("false", result));
    EXPECT_FALSE(result);
    EXPECT_TRUE(tools::getBoolValue("False", result));
    EXPECT_FALSE(result);
    EXPECT_TRUE(tools::getBoolValue("fAlSe", result));
    EXPECT_FALSE(result);
    EXPECT_TRUE(tools::getBoolValue("FALSE", result));
    EXPECT_FALSE(result);
}

TEST(StringTest, GetBoolValueTest_True) {
    bool result = false;
    EXPECT_TRUE(tools::getBoolValue("true", result));
    EXPECT_TRUE(result);
    EXPECT_TRUE(tools::getBoolValue("True", result));
    EXPECT_TRUE(result);
    EXPECT_TRUE(tools::getBoolValue("tRuE", result));
    EXPECT_TRUE(result);
    EXPECT_TRUE(tools::getBoolValue("TRUE", result));
    EXPECT_TRUE(result);
}

TEST(StringTest, GetBoolValueTest_DifferentStartingValue) {
    bool result = true;
    EXPECT_TRUE(tools::getBoolValue("1", result));
    EXPECT_TRUE(result);
    EXPECT_TRUE(tools::getBoolValue("0", result));
    EXPECT_FALSE(result);
}

TEST(StringTest, GetBoolValueTest_InvalidValue) {
    bool result = false;
    EXPECT_FALSE(tools::getBoolValue("foo", result));
    EXPECT_FALSE(result);
    result = true;
    EXPECT_FALSE(tools::getBoolValue("foo", result));
    EXPECT_TRUE(result);
}

TEST(StringTest, IntToStringTest_SingleDigit) {
    EXPECT_EQ(tools::intToString(5), "5");
}

TEST(StringTest, IntToStringTest_Zero) {
    EXPECT_EQ(tools::intToString(0), "0");
}

TEST(StringTest, IntToStringTest_MultipleDigits) {
    EXPECT_EQ(tools::intToString(23523), "23523");
}

TEST(StringTest, IntToStringTest_NegativeNumber) {
    EXPECT_EQ(tools::intToString(-125), "-125");
}

TEST(StringTest, IntToStringTest_Hex) {
    EXPECT_EQ(tools::intToString(0x12dead, 16), "12DEAD");
}

TEST(StringTest, IntToStringTest_Oct) {
    EXPECT_EQ(tools::intToString(0137235, 8), "137235");
}

TEST(StringTest, FloatToStringTest_Integer) {
    EXPECT_EQ(tools::floatToString(42.0, 2), "42");
}

TEST(StringTest, FloatToStringTest_StopAtPoints) {
    EXPECT_EQ(tools::floatToString(51.34123, 2), "51.34");
}

TEST(StringTest, FloatToStringTest_Negative) {
    EXPECT_EQ(tools::floatToString(-42.6101, 2), "-42.61");
}

TEST(StringTest, FloatToStringTest_SmallPositive) {
    EXPECT_EQ(tools::floatToString(0.123, 2), "0.12");
}

TEST(StringTest, FloatToStringTest_SmallNegative) {
    EXPECT_EQ(tools::floatToString(-0.321, 2), "-0.32");
}

TEST(StringTest, FloatToStringTest_VerySmallPositive) {
    EXPECT_EQ(tools::floatToString(0.0001, 2), "0.00");
}

TEST(StringTest, FloatToStringTest_VerySmallNegative) {
    EXPECT_EQ(tools::floatToString(-0.0001, 2), "-0.00");
}

TEST(StringTest, FloatToStringTest_MoreComplicated) {
    EXPECT_EQ(tools::floatToString(1.0 / 3.0, 6), "0.333333");
}
