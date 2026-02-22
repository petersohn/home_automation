#include <boost/test/unit_test.hpp>

#include "EspTestBase.hpp"
#include "common/InterfaceConfig.hpp"
#include "operation/OperationParser2.hpp"

BOOST_AUTO_TEST_SUITE(OperationParser2Test)

struct Fixture : EspTestBase {
    std::vector<std::unique_ptr<InterfaceConfig>> interfaces;

    void addInterface(std::string name, std::vector<std::string> values) {
        interfaces.emplace_back(std::make_unique<InterfaceConfig>());
        interfaces.back()->name = std::move(name);
        interfaces.back()->storedValue = std::move(values);
    }
};

BOOST_FIXTURE_TEST_CASE(ConstantString, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("'foobar'");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "foobar");
}

BOOST_FIXTURE_TEST_CASE(ConstantNumber, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("123");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "123");
}

BOOST_FIXTURE_TEST_CASE(ConstantFractionalNumber, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("0.321");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "0.321");
}

BOOST_FIXTURE_TEST_CASE(ConstantFalse, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("false");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "0");
}

BOOST_FIXTURE_TEST_CASE(ConstantTrue, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("true");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "1");
}

BOOST_FIXTURE_TEST_CASE(ConstantOff, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("off");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "0");
}

BOOST_FIXTURE_TEST_CASE(ConstantOn, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("on");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "1");
}

BOOST_FIXTURE_TEST_CASE(StringLiteralWithEscapeSequences, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse(R"('foo\'bar"foobar\\baz')");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == R"(foo'bar"foobar\baz)");
}

BOOST_FIXTURE_TEST_CASE(NumericalAddition, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("3 + 5");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "8");
}

BOOST_FIXTURE_TEST_CASE(NumericalSubtraction, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("3 - 5");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "-2");
}

BOOST_FIXTURE_TEST_CASE(NumericalMultiplication, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("3 * 5");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "15");
}

BOOST_FIXTURE_TEST_CASE(NumericalDivision, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("9 / 2");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "4.5");
}

BOOST_FIXTURE_TEST_CASE(ConvertStringToNumber, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("'3' + '9'");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "12");
}

BOOST_FIXTURE_TEST_CASE(NumericConversionErrorShouldResultZero, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("3 + 'foo'");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "3");
}

BOOST_FIXTURE_TEST_CASE(EmptyStringShouldResultZero, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("3 + ''");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "3");
}

BOOST_FIXTURE_TEST_CASE(StringConcatenation, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("'foo' s+ 'bar'");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "foobar");
}

BOOST_FIXTURE_TEST_CASE(ConcatenateNumbers, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("5 s+ 11");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "511");
}

BOOST_FIXTURE_TEST_CASE(NegativeNumbers, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("-3 + -8");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "-11");
}

BOOST_FIXTURE_TEST_CASE(OperandsWithDifferentSpacing, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("3+2  - 4");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "1");
}

BOOST_FIXTURE_TEST_CASE(NegativeNumbersWithDifferentSpacing, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("10+-6--9");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "13");  // 10 + -6 - -9 = 10 - 6 + 9
}

BOOST_FIXTURE_TEST_CASE(PrecedenceMultiplyOverAdd, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("11 + 3 * 5 + 2");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "28");  // 11 + 15 + 2
}

BOOST_FIXTURE_TEST_CASE(PrecedenceDivideOverAdd, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("11 + 6 / 3 + 5");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "18");  // 11 + 2 + 5
}

BOOST_FIXTURE_TEST_CASE(PrecedenceMultiplyOverSubtract, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("11 - 3 * 5 - 2");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "-6");  // 11 - 15 - 2
}

BOOST_FIXTURE_TEST_CASE(PrecedenceDivideOverSubtract, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("11 - 6 / 3 - 5");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "4");  // 11 - 2 - 5
}

BOOST_FIXTURE_TEST_CASE(PrecedenceAddAndSubtractAreTheSame, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("10 + 3 - 5 + 9");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "17");
}

BOOST_FIXTURE_TEST_CASE(PrecedenceMultiplyAndDivideAreTheSame, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("15 * 3 / 5 * 2");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "18");
}

BOOST_FIXTURE_TEST_CASE(GroupingWithParenthesis, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("(3 + 5) * (9 - 12)");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "-24");  // 8 * -3
}

BOOST_FIXTURE_TEST_CASE(DefaultValueOfInterface, Fixture) {
    addInterface("itf1", {"foo", "bar"});
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("[itf1]");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "foo");
}

BOOST_FIXTURE_TEST_CASE(MultipleValuesOfInterface, Fixture) {
    addInterface("itf1", {"foo", "bar"});
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("[itf1].1 s+ [itf1].2");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "foobar");
}

BOOST_FIXTURE_TEST_CASE(MultipleInterfaces, Fixture) {
    addInterface("itf1", {"foo"});
    addInterface("itf2", {"bar"});
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("[itf1] s+ [itf2]");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "foobar");
}

BOOST_FIXTURE_TEST_CASE(DefaultInterface, Fixture) {
    addInterface("itf1", {"foo", "bar"});
    addInterface("itf2", {"baz"});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 s+ %2");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "foobar");
}

BOOST_FIXTURE_TEST_CASE(DefaultAndNonDefaultInterfaces, Fixture) {
    addInterface("itf1", {"foo", "bar"});
    addInterface("itf2", {"baz"});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 s+ [itf2]");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "foobaz");
}

BOOST_FIXTURE_TEST_CASE(InterfaceWithMoreComplexName, Fixture) {
    addInterface("a b+c  ", {"foo"});
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse("[a b+c  ]");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "foo");
}

BOOST_FIXTURE_TEST_CASE(InterfaceNameEscaping, Fixture) {
    addInterface(R"(itf]-\)", {"foo"});
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto operation = parser.parse(R"([itf\]-\\])");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "foo");
}

BOOST_FIXTURE_TEST_CASE(NumericOperationWithInterfaces, Fixture) {
    addInterface("itf1", {"5", "3"});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 + %2");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "8");
}

BOOST_FIXTURE_TEST_CASE(NumericEqualComparison, Fixture) {
    addInterface("itf1", {"0"});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 == 50");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "50";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "0050";
    BOOST_TEST(operation->evaluate() == "1");
}

BOOST_FIXTURE_TEST_CASE(NumericNotEqualComparison, Fixture) {
    addInterface("itf1", {"0"});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 != 50");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "50";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "0050";
    BOOST_TEST(operation->evaluate() == "0");
}

BOOST_FIXTURE_TEST_CASE(NumericLessThanComparison, Fixture) {
    addInterface("itf1", {"0"});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 < 50");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "49";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "049";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "50";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "51";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "051";
    BOOST_TEST(operation->evaluate() == "0");
}

BOOST_FIXTURE_TEST_CASE(NumericLessThanOrEqualComparison, Fixture) {
    addInterface("itf1", {"0"});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 <= 50");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "49";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "049";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "50";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "51";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "051";
    BOOST_TEST(operation->evaluate() == "0");
}

BOOST_FIXTURE_TEST_CASE(NumericLessGreaterThanComparison, Fixture) {
    addInterface("itf1", {"0"});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 > 50");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "49";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "049";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "50";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "51";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "051";
    BOOST_TEST(operation->evaluate() == "1");
}

BOOST_FIXTURE_TEST_CASE(NumericLessGreaterThanOrEqualComparison, Fixture) {
    addInterface("itf1", {"0"});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 >= 50");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "49";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "049";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "50";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "51";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "051";
    BOOST_TEST(operation->evaluate() == "1");
}

BOOST_FIXTURE_TEST_CASE(StringEqualComparison, Fixture) {
    addInterface("itf1", {""});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 s== 'foo'");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "foo";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "fo";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "fooo";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "bar";
    BOOST_TEST(operation->evaluate() == "0");
}

BOOST_FIXTURE_TEST_CASE(StringLessThanComparison, Fixture) {
    addInterface("itf1", {""});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 s< 'foo'");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "fo";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "faa";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "foo";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "fooa";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "fou";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "zzz";
    BOOST_TEST(operation->evaluate() == "0");
}

BOOST_FIXTURE_TEST_CASE(StringLessThanOrEqualComparison, Fixture) {
    addInterface("itf1", {""});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 s<= 'foo'");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "fo";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "faa";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "foo";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "fooa";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "fou";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "zzz";
    BOOST_TEST(operation->evaluate() == "0");
}

BOOST_FIXTURE_TEST_CASE(StringGreaterThanComparison, Fixture) {
    addInterface("itf1", {""});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 s> 'foo'");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "fo";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "faa";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "foo";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "fooa";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "fou";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "zzz";
    BOOST_TEST(operation->evaluate() == "1");
}

BOOST_FIXTURE_TEST_CASE(StringGreaterThanOrEqualComparison, Fixture) {
    addInterface("itf1", {""});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 s>= 'foo'");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "fo";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "faa";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "foo";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "fooa";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "fou";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "zzz";
    BOOST_TEST(operation->evaluate() == "1");
}

BOOST_FIXTURE_TEST_CASE(StringComparisonWithNumbers, Fixture) {
    addInterface("itf1", {""});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 s== '50'");
    BOOST_REQUIRE(operation != nullptr);
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "50";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "0050";
    BOOST_TEST(operation->evaluate() == "0");
}

BOOST_FIXTURE_TEST_CASE(LogicalAnd, Fixture) {
    addInterface("itf1", {"", ""});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 && %2");
    BOOST_REQUIRE(operation != nullptr);
    interfaces[0]->storedValue[0] = "false";
    interfaces[0]->storedValue[1] = "false";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "true";
    interfaces[0]->storedValue[1] = "false";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "false";
    interfaces[0]->storedValue[1] = "true";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "true";
    interfaces[0]->storedValue[1] = "true";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "0";
    interfaces[0]->storedValue[1] = "true";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "1";
    interfaces[0]->storedValue[1] = "true";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "off";
    interfaces[0]->storedValue[1] = "true";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "on";
    interfaces[0]->storedValue[1] = "true";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "foo";
    interfaces[0]->storedValue[1] = "true";
    BOOST_TEST(operation->evaluate() == "0");
}

BOOST_FIXTURE_TEST_CASE(LogicalOr, Fixture) {
    addInterface("itf1", {"", ""});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 || %2");
    BOOST_REQUIRE(operation != nullptr);
    interfaces[0]->storedValue[0] = "false";
    interfaces[0]->storedValue[1] = "false";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "true";
    interfaces[0]->storedValue[1] = "false";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "false";
    interfaces[0]->storedValue[1] = "true";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "true";
    interfaces[0]->storedValue[1] = "true";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "0";
    interfaces[0]->storedValue[1] = "false";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "1";
    interfaces[0]->storedValue[1] = "false";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "off";
    interfaces[0]->storedValue[1] = "false";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "on";
    interfaces[0]->storedValue[1] = "false";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "foo";
    interfaces[0]->storedValue[1] = "false";
    BOOST_TEST(operation->evaluate() == "0");
}

BOOST_FIXTURE_TEST_CASE(LogicalNot, Fixture) {
    addInterface("itf1", {""});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("!%1");
    BOOST_REQUIRE(operation != nullptr);
    interfaces[0]->storedValue[0] = "false";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "true";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "0";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "1";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "off";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "on";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "5";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "-5";
    BOOST_TEST(operation->evaluate() == "1");
    interfaces[0]->storedValue[0] = "foo";
    BOOST_TEST(operation->evaluate() == "1");
}

BOOST_FIXTURE_TEST_CASE(DoubleLogicalNot, Fixture) {
    addInterface("itf1", {""});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("!!%1");
    BOOST_REQUIRE(operation != nullptr);
    interfaces[0]->storedValue[0] = "false";
    BOOST_TEST(operation->evaluate() == "0");
    interfaces[0]->storedValue[0] = "true";
    BOOST_TEST(operation->evaluate() == "1");
}

BOOST_FIXTURE_TEST_CASE(ConditionalExpression, Fixture) {
    addInterface("itf1", {""});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 ? 'foo' : 'bar'");
    BOOST_REQUIRE(operation != nullptr);
    interfaces[0]->storedValue[0] = "true";
    BOOST_TEST(operation->evaluate() == "foo");
    interfaces[0]->storedValue[0] = "false";
    BOOST_TEST(operation->evaluate() == "bar");
}

BOOST_FIXTURE_TEST_CASE(ConditionalExpressionHasLowPrecedence, Fixture) {
    addInterface("itf1", {""});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 < 0 ? 3 + 5 : 9 * 8");
    BOOST_REQUIRE(operation != nullptr);
    interfaces[0]->storedValue[0] = "-2";
    BOOST_TEST(operation->evaluate() == "8");
    interfaces[0]->storedValue[0] = "2";
    BOOST_TEST(operation->evaluate() == "72");
}

BOOST_FIXTURE_TEST_CASE(ChainedConditionalExpressions, Fixture) {
    addInterface("itf1", {"", ""});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 ? 'foo' : %2 ? 'bar' : 'baz'");
    BOOST_REQUIRE(operation != nullptr);
    interfaces[0]->storedValue[0] = "true";
    interfaces[0]->storedValue[1] = "true";
    BOOST_TEST(operation->evaluate() == "foo");
    interfaces[0]->storedValue[0] = "true";
    interfaces[0]->storedValue[1] = "false";
    BOOST_TEST(operation->evaluate() == "foo");
    interfaces[0]->storedValue[0] = "false";
    interfaces[0]->storedValue[1] = "true";
    BOOST_TEST(operation->evaluate() == "bar");
    interfaces[0]->storedValue[0] = "false";
    interfaces[0]->storedValue[1] = "false";
    BOOST_TEST(operation->evaluate() == "baz");
}

BOOST_FIXTURE_TEST_CASE(GroupingOfConditionalExpressions, Fixture) {
    addInterface("itf1", {""});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("10 + (%1 ? 3 : -2) * 2");
    BOOST_REQUIRE(operation != nullptr);
    interfaces[0]->storedValue[0] = "true";
    BOOST_TEST(operation->evaluate() == "16");  // 10 + 3 * 2
    interfaces[0]->storedValue[0] = "false";
    BOOST_TEST(operation->evaluate() == "6");  // 10 + (-2) * 2
}

BOOST_FIXTURE_TEST_CASE(
    ConditionalExpressionWithinAnotherConditional, Fixture) {
    addInterface("itf1", {"", ""});
    operation::Parser2 parser{debug, interfaces, interfaces[0].get()};
    auto operation = parser.parse("%1 ? (%2 ? 'a' : 'b') : (%2 ? 'c' : 'd')");
    BOOST_REQUIRE(operation != nullptr);
    interfaces[0]->storedValue[0] = "true";
    interfaces[0]->storedValue[1] = "true";
    BOOST_TEST(operation->evaluate() == "a");
    interfaces[0]->storedValue[0] = "true";
    interfaces[0]->storedValue[1] = "false";
    BOOST_TEST(operation->evaluate() == "b");
    interfaces[0]->storedValue[0] = "false";
    interfaces[0]->storedValue[1] = "true";
    BOOST_TEST(operation->evaluate() == "c");
    interfaces[0]->storedValue[0] = "false";
    interfaces[0]->storedValue[1] = "false";
    BOOST_TEST(operation->evaluate() == "d");
}

BOOST_FIXTURE_TEST_CASE(SyntaxErrorEmptyExpression, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("");
    BOOST_REQUIRE(operation == nullptr);
}

BOOST_FIXTURE_TEST_CASE(SyntaxErrorUnfinishedExpression, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("5 +");
    BOOST_REQUIRE(operation == nullptr);
}

BOOST_FIXTURE_TEST_CASE(SyntaxErrorUnmatchedQuote, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("'foo");
    BOOST_REQUIRE(operation == nullptr);
}

BOOST_FIXTURE_TEST_CASE(SyntaxErrorEmptyParenthesis, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("()");
    BOOST_REQUIRE(operation == nullptr);
}

BOOST_FIXTURE_TEST_CASE(SyntaxErrorUnmatchedClosingParenthesis, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("(3 + 6");
    BOOST_REQUIRE(operation == nullptr);
}

BOOST_FIXTURE_TEST_CASE(SyntaxErrorUnmatchedOpeningParenthesis, Fixture) {
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("(3 + 6");
    BOOST_REQUIRE(operation == nullptr);
}

BOOST_FIXTURE_TEST_CASE(SyntaxErrorUnmatchedClosingBracket, Fixture) {
    addInterface("itf1", {""});
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("[itf1");
    BOOST_REQUIRE(operation == nullptr);
}

BOOST_FIXTURE_TEST_CASE(SyntaxErrorUnmatchedOpeningBracket, Fixture) {
    addInterface("itf1", {""});
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("itf1]");
    BOOST_REQUIRE(operation == nullptr);
}

BOOST_FIXTURE_TEST_CASE(SyntaxErrorBadValueNumber, Fixture) {
    addInterface("itf1", {""});
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("[itf1].a");
    BOOST_REQUIRE(operation == nullptr);
}

BOOST_FIXTURE_TEST_CASE(SyntaxErrorMissingValueNumber, Fixture) {
    addInterface("itf1", {""});
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto ex = expectLog("Syntax error:");
    auto operation = parser.parse("[itf1].");
    BOOST_REQUIRE(operation == nullptr);
}

BOOST_FIXTURE_TEST_CASE(ErrorMissingInterface, Fixture) {
    addInterface("itf1", {""});
    operation::Parser2 parser{debug, interfaces, nullptr};
    auto ex = expectLog("Error:");
    auto operation = parser.parse("[itf2]");
    BOOST_REQUIRE(operation == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
