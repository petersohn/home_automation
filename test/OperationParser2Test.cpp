#include <gtest/gtest.h>

#include <iostream>

#include "EspTestBase.hpp"
#include "common/InterfaceConfig.hpp"
#include "operation/OperationParser2.hpp"

struct OperationParser2Test : EspTestBase {
    std::vector<std::unique_ptr<InterfaceConfig>> interfaces;

    void addInterface(std::string name, std::vector<std::string> values) {
        this->interfaces.emplace_back(std::make_unique<InterfaceConfig>());
        this->interfaces.back()->name = std::move(name);
        this->interfaces.back()->storedValue = std::move(values);
    }
};

TEST_F(OperationParser2Test, ConstantString) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("'foobar'");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "foobar");
}

TEST_F(OperationParser2Test, ConstantNumber) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("123");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "123");
}

TEST_F(OperationParser2Test, ConstantFractionalNumber) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("0.321");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "0.321");
}

TEST_F(OperationParser2Test, ConstantFalse) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("false");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "0");
}

TEST_F(OperationParser2Test, ConstantTrue) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("true");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "1");
}

TEST_F(OperationParser2Test, ConstantOff) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("off");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "0");
}

TEST_F(OperationParser2Test, ConstantOn) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("on");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "1");
}

TEST_F(OperationParser2Test, StringLiteralWithEscapeSequences) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse(R"('foo\'bar"foobar\\baz')");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), R"(foo'bar"foobar\baz)");
}

TEST_F(OperationParser2Test, NumericalAddition) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("3 + 5");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "8");
}

TEST_F(OperationParser2Test, NumericalSubtraction) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("3 - 5");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "-2");
}

TEST_F(OperationParser2Test, NumericalMultiplication) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("3 * 5");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "15");
}

TEST_F(OperationParser2Test, NumericalDivision) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("9 / 2");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "4.5");
}

TEST_F(OperationParser2Test, ConvertStringToNumber) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("'3' + '9'");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "12");
}

TEST_F(OperationParser2Test, NumericConversionErrorShouldResultZero) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("3 + 'foo'");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "3");
}

TEST_F(OperationParser2Test, EmptyStringShouldResultZero) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("3 + ''");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "3");
}

TEST_F(OperationParser2Test, StringConcatenation) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("'foo' s+ 'bar'");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "foobar");
}

TEST_F(OperationParser2Test, ConcatenateNumbers) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("5 s+ 11");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "511");
}

TEST_F(OperationParser2Test, NegativeNumbers) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("-3 + -8");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "-11");
}

TEST_F(OperationParser2Test, OperandsWithDifferentSpacing) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("3+2  - 4");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "1");
}

TEST_F(OperationParser2Test, NegativeNumbersWithDifferentSpacing) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("10+-6--9");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "13");  // 10 + -6 - -9 = 10 - 6 + 9
}

TEST_F(OperationParser2Test, PrecedenceMultiplyOverAdd) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("11 + 3 * 5 + 2");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "28");  // 11 + 15 + 2
}

TEST_F(OperationParser2Test, PrecedenceDivideOverAdd) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("11 + 6 / 3 + 5");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "18");  // 11 + 2 + 5
}

TEST_F(OperationParser2Test, PrecedenceMultiplyOverSubtract) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("11 - 3 * 5 - 2");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "-6");  // 11 - 15 - 2
}

TEST_F(OperationParser2Test, PrecedenceDivideOverSubtract) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("11 - 6 / 3 - 5");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "4");  // 11 - 2 - 5
}

TEST_F(OperationParser2Test, PrecedenceAddAndSubtractAreTheSame) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("10 + 3 - 5 + 9");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "17");
}

TEST_F(OperationParser2Test, PrecedenceMultiplyAndDivideAreTheSame) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("15 * 3 / 5 * 2");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "18");
}

TEST_F(OperationParser2Test, GroupingWithParenthesis) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("(3 + 5) * (9 - 12)");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "-24");  // 8 * -3
}

TEST_F(OperationParser2Test, DefaultValueOfInterface) {
    addInterface("itf1", {"foo", "bar"});
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("[itf1]");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "foo");
}

TEST_F(OperationParser2Test, MultipleValuesOfInterface) {
    addInterface("itf1", {"foo", "bar"});
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("[itf1].1 s+ [itf1].2");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "foobar");
}

TEST_F(OperationParser2Test, MultipleInterfaces) {
    addInterface("itf1", {"foo"});
    addInterface("itf2", {"bar"});
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("[itf1] s+ [itf2]");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "foobar");
}

TEST_F(OperationParser2Test, DefaultInterface) {
    addInterface("itf1", {"foo", "bar"});
    addInterface("itf2", {"baz"});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 s+ %2");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "foobar");
}

TEST_F(OperationParser2Test, DefaultAndNonDefaultInterfaces) {
    addInterface("itf1", {"foo", "bar"});
    addInterface("itf2", {"baz"});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 s+ [itf2]");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "foobaz");
}

TEST_F(OperationParser2Test, InterfaceWithMoreComplexName) {
    addInterface("a b+c  ", {"foo"});
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse("[a b+c  ]");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "foo");
}

TEST_F(OperationParser2Test, InterfaceNameEscaping) {
    addInterface(R"(itf]-\)", {"foo"});
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto operation = parser.parse(R"([itf\]-\\])");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "foo");
}

TEST_F(OperationParser2Test, NumericOperationWithInterfaces) {
    addInterface("itf1", {"5", "3"});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 + %2");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "8");
}

TEST_F(OperationParser2Test, NumericEqualComparison) {
    this->addInterface("itf1", {"0"});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 == 50");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "50";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "0050";
    EXPECT_EQ(operation->evaluate(), "1");
}

TEST_F(OperationParser2Test, NumericNotEqualComparison) {
    this->addInterface("itf1", {"0"});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 != 50");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "50";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "0050";
    EXPECT_EQ(operation->evaluate(), "0");
}

TEST_F(OperationParser2Test, NumericLessThanComparison) {
    this->addInterface("itf1", {"0"});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 < 50");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "49";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "049";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "50";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "51";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "051";
    EXPECT_EQ(operation->evaluate(), "0");
}

TEST_F(OperationParser2Test, NumericLessThanOrEqualComparison) {
    this->addInterface("itf1", {"0"});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 <= 50");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "49";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "049";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "50";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "51";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "051";
    EXPECT_EQ(operation->evaluate(), "0");
}

TEST_F(OperationParser2Test, NumericLessGreaterThanComparison) {
    this->addInterface("itf1", {"0"});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 > 50");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "49";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "049";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "50";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "51";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "051";
    EXPECT_EQ(operation->evaluate(), "1");
}

TEST_F(OperationParser2Test, NumericLessGreaterThanOrEqualComparison) {
    this->addInterface("itf1", {"0"});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 >= 50");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "49";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "049";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "50";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "51";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "051";
    EXPECT_EQ(operation->evaluate(), "1");
}

TEST_F(OperationParser2Test, StringEqualComparison) {
    this->addInterface("itf1", {""});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 s== 'foo'");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "foo";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "fo";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "fooo";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "bar";
    EXPECT_EQ(operation->evaluate(), "0");
}

TEST_F(OperationParser2Test, StringLessThanComparison) {
    this->addInterface("itf1", {""});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 s< 'foo'");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "fo";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "faa";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "foo";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "fooa";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "fou";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "zzz";
    EXPECT_EQ(operation->evaluate(), "0");
}

TEST_F(OperationParser2Test, StringLessThanOrEqualComparison) {
    this->addInterface("itf1", {""});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 s<= 'foo'");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "fo";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "faa";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "foo";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "fooa";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "fou";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "zzz";
    EXPECT_EQ(operation->evaluate(), "0");
}

TEST_F(OperationParser2Test, StringGreaterThanComparison) {
    this->addInterface("itf1", {""});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 s> 'foo'");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "fo";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "faa";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "foo";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "fooa";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "fou";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "zzz";
    EXPECT_EQ(operation->evaluate(), "1");
}

TEST_F(OperationParser2Test, StringGreaterThanOrEqualComparison) {
    this->addInterface("itf1", {""});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 s>= 'foo'");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "fo";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "faa";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "foo";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "fooa";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "fou";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "zzz";
    EXPECT_EQ(operation->evaluate(), "1");
}

TEST_F(OperationParser2Test, StringComparisonWithNumbers) {
    addInterface("itf1", {""});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 s== '50'");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "50";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "0050";
    EXPECT_EQ(operation->evaluate(), "0");
}

TEST_F(OperationParser2Test, LogicalAnd) {
    addInterface("itf1", {"", ""});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 && %2");
    ASSERT_NE(operation, nullptr);
    this->interfaces[0]->storedValue[0] = "false";
    this->interfaces[0]->storedValue[1] = "false";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "true";
    this->interfaces[0]->storedValue[1] = "false";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "false";
    this->interfaces[0]->storedValue[1] = "true";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "true";
    this->interfaces[0]->storedValue[1] = "true";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "0";
    this->interfaces[0]->storedValue[1] = "true";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "1";
    this->interfaces[0]->storedValue[1] = "true";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "off";
    this->interfaces[0]->storedValue[1] = "true";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "on";
    this->interfaces[0]->storedValue[1] = "true";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "foo";
    this->interfaces[0]->storedValue[1] = "true";
    EXPECT_EQ(operation->evaluate(), "0");
}

TEST_F(OperationParser2Test, LogicalOr) {
    addInterface("itf1", {"", ""});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 || %2");
    ASSERT_NE(operation, nullptr);
    this->interfaces[0]->storedValue[0] = "false";
    this->interfaces[0]->storedValue[1] = "false";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "true";
    this->interfaces[0]->storedValue[1] = "false";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "false";
    this->interfaces[0]->storedValue[1] = "true";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "true";
    this->interfaces[0]->storedValue[1] = "true";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "0";
    this->interfaces[0]->storedValue[1] = "false";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "1";
    this->interfaces[0]->storedValue[1] = "false";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "off";
    this->interfaces[0]->storedValue[1] = "false";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "on";
    this->interfaces[0]->storedValue[1] = "false";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "foo";
    this->interfaces[0]->storedValue[1] = "false";
    EXPECT_EQ(operation->evaluate(), "0");
}

TEST_F(OperationParser2Test, LogicalNot) {
    addInterface("itf1", {""});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("!%1");
    ASSERT_NE(operation, nullptr);
    this->interfaces[0]->storedValue[0] = "false";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "true";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "0";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "1";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "off";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "on";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "5";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "-5";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "foo";
    EXPECT_EQ(operation->evaluate(), "1");
}

TEST_F(OperationParser2Test, DoubleLogicalNot) {
    addInterface("itf1", {""});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("!!%1");
    ASSERT_NE(operation, nullptr);
    this->interfaces[0]->storedValue[0] = "false";
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "true";
    EXPECT_EQ(operation->evaluate(), "1");
}

TEST_F(OperationParser2Test, PrecedenceOfDifferentLogicalOperators) {
    addInterface("itf1", {"0", "0", "0", "0"});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 && %2 || %3 && %4");
    ASSERT_NE(operation, nullptr);
    this->interfaces[0]->storedValue = {"0", "0", "0", "0"};
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue = {"1", "0", "0", "0"};
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue = {"0", "1", "0", "0"};
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue = {"1", "1", "0", "0"};
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue = {"0", "0", "1", "0"};
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue = {"1", "0", "1", "0"};
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue = {"0", "1", "1", "0"};
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue = {"1", "1", "1", "0"};
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue = {"0", "0", "0", "1"};
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue = {"1", "0", "0", "1"};
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue = {"0", "1", "0", "1"};
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue = {"1", "1", "0", "1"};
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue = {"0", "0", "1", "1"};
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue = {"1", "0", "1", "1"};
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue = {"0", "1", "1", "1"};
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue = {"1", "1", "1", "1"};
    EXPECT_EQ(operation->evaluate(), "1");
}

TEST_F(OperationParser2Test, PrecedenceOfLogicalAndComparisonOperations) {
    addInterface("itf1", {"0", "0"});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 < -5 || %1 > 10 || %2 <= -5 || %2 >= 10");
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(operation->evaluate(), "0");
    this->interfaces[0]->storedValue[0] = "-6";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "11";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[0] = "0";
    this->interfaces[0]->storedValue[1] = "-5";
    EXPECT_EQ(operation->evaluate(), "1");
    this->interfaces[0]->storedValue[1] = "10";
    EXPECT_EQ(operation->evaluate(), "1");
}

TEST_F(OperationParser2Test, ConditionalExpression) {
    addInterface("itf1", {""});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 ? 'foo' : 'bar'");
    ASSERT_NE(operation, nullptr);
    this->interfaces[0]->storedValue[0] = "true";
    EXPECT_EQ(operation->evaluate(), "foo");
    this->interfaces[0]->storedValue[0] = "false";
    EXPECT_EQ(operation->evaluate(), "bar");
}

TEST_F(OperationParser2Test, ConditionalExpressionHasLowPrecedence) {
    addInterface("itf1", {""});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 < 0 ? 3 + 5 : 9 * 8");
    ASSERT_NE(operation, nullptr);
    this->interfaces[0]->storedValue[0] = "-2";
    EXPECT_EQ(operation->evaluate(), "8");
    this->interfaces[0]->storedValue[0] = "2";
    EXPECT_EQ(operation->evaluate(), "72");
}

TEST_F(OperationParser2Test, ChainedConditionalExpressions) {
    addInterface("itf1", {"", ""});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 ? 'foo' : %2 ? 'bar' : 'baz'");
    ASSERT_NE(operation, nullptr);
    this->interfaces[0]->storedValue[0] = "true";
    this->interfaces[0]->storedValue[1] = "true";
    EXPECT_EQ(operation->evaluate(), "foo");
    this->interfaces[0]->storedValue[0] = "true";
    this->interfaces[0]->storedValue[1] = "false";
    EXPECT_EQ(operation->evaluate(), "foo");
    this->interfaces[0]->storedValue[0] = "false";
    this->interfaces[0]->storedValue[1] = "true";
    EXPECT_EQ(operation->evaluate(), "bar");
    this->interfaces[0]->storedValue[0] = "false";
    this->interfaces[0]->storedValue[1] = "false";
    EXPECT_EQ(operation->evaluate(), "baz");
}

TEST_F(OperationParser2Test, GroupingOfConditionalExpressions) {
    addInterface("itf1", {""});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("10 + (%1 ? 3 : -2) * 2");
    ASSERT_NE(operation, nullptr);
    this->interfaces[0]->storedValue[0] = "true";
    EXPECT_EQ(operation->evaluate(), "16");  // 10 + 3 * 2
    this->interfaces[0]->storedValue[0] = "false";
    EXPECT_EQ(operation->evaluate(), "6");  // 10 + (-2) * 2
}

TEST_F(OperationParser2Test, ConditionalExpressionWithinAnotherConditional) {
    addInterface("itf1", {"", ""});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 ? (%2 ? 'a' : 'b') : (%2 ? 'c' : 'd')");
    ASSERT_NE(operation, nullptr);
    this->interfaces[0]->storedValue[0] = "true";
    this->interfaces[0]->storedValue[1] = "true";
    EXPECT_EQ(operation->evaluate(), "a");
    this->interfaces[0]->storedValue[0] = "true";
    this->interfaces[0]->storedValue[1] = "false";
    EXPECT_EQ(operation->evaluate(), "b");
    this->interfaces[0]->storedValue[0] = "false";
    this->interfaces[0]->storedValue[1] = "true";
    EXPECT_EQ(operation->evaluate(), "c");
    this->interfaces[0]->storedValue[0] = "false";
    this->interfaces[0]->storedValue[1] = "false";
    EXPECT_EQ(operation->evaluate(), "d");
}

TEST_F(OperationParser2Test, UsedInterfaces) {
    this->addInterface("itf1", {"0"});
    this->addInterface("itf2", {"0"});
    this->addInterface("itf3", {"0"});
    operation::Parser2 parser{
        this->debug, this->interfaces, this->interfaces[0].get()};
    auto operation = parser.parse("%1 + [itf2].1");
    ASSERT_NE(operation, nullptr);
    for (const auto* itf : parser.getUsedInterfaces()) {
        std::cout << itf->name << std::endl;
    }
    std::unordered_set<InterfaceConfig*> expectedUsedInterfaces{
        this->interfaces[0].get(), this->interfaces[1].get()};
    EXPECT_EQ(parser.getUsedInterfaces(), expectedUsedInterfaces);
}

TEST_F(OperationParser2Test, SyntaxErrorEmptyExpression) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("");
    ASSERT_EQ(operation, nullptr);
}

TEST_F(OperationParser2Test, SyntaxErrorUnfinishedExpression) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("5 +");
    ASSERT_EQ(operation, nullptr);
}

TEST_F(OperationParser2Test, SyntaxErrorUnmatchedQuote) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("'foo");
    ASSERT_EQ(operation, nullptr);
}

TEST_F(OperationParser2Test, SyntaxErrorEmptyParenthesis) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("()");
    ASSERT_EQ(operation, nullptr);
}

TEST_F(OperationParser2Test, SyntaxErrorUnmatchedClosingParenthesis) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("(3 + 6");
    ASSERT_EQ(operation, nullptr);
}

TEST_F(OperationParser2Test, SyntaxErrorUnmatchedOpeningParenthesis) {
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("(3 + 6");
    ASSERT_EQ(operation, nullptr);
}

TEST_F(OperationParser2Test, SyntaxErrorUnmatchedClosingBracket) {
    addInterface("itf1", {""});
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("[itf1");
    ASSERT_EQ(operation, nullptr);
}

TEST_F(OperationParser2Test, SyntaxErrorUnmatchedOpeningBracket) {
    addInterface("itf1", {""});
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("itf1]");
    ASSERT_EQ(operation, nullptr);
}

TEST_F(OperationParser2Test, SyntaxErrorBadValueNumber) {
    addInterface("itf1", {""});
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("[itf1].a");
    ASSERT_EQ(operation, nullptr);
}

TEST_F(OperationParser2Test, SyntaxErrorMissingValueNumber) {
    addInterface("itf1", {""});
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("[itf1].");
    ASSERT_EQ(operation, nullptr);
}

TEST_F(OperationParser2Test, ErrorMissingInterface) {
    addInterface("itf1", {""});
    operation::Parser2 parser{this->debug, this->interfaces, nullptr};
    auto ex = expectLog("Error:");
    auto operation = parser.parse("[itf2]");
    ASSERT_EQ(operation, nullptr);
}
