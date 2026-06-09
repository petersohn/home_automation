#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <variant>

#include "EspTestBase.hpp"
#include "common/ArduinoJson.hpp"
#include "common/Interface.hpp"
#include "operation/OperationParser.hpp"

using namespace ArduinoJson;

struct OperationTestSampleString {
    std::string operation;
    std::string operands;
    std::string expectedValue;
};

struct OperationTestSampleNumber {
    std::string operation;
    std::string operands;
    float expectedValue;
};

struct OperationTestSampleConditional {
    std::string condition;
    std::string thenBranch;
    std::string elseBranch;
    std::string expectedValue;
};

struct OperationTestSampleMapping {
    std::string operation;
    std::string operands;
    std::string value;
    std::string expectedValue;
};

std::ostream& operator<<(
    std::ostream& os, const OperationTestSampleString& sample) {
    return os << sample.operation << "(" << sample.operands
              << ") = " << sample.expectedValue;
}

std::ostream& operator<<(
    std::ostream& os, const OperationTestSampleNumber& sample) {
    return os << sample.operation << "(" << sample.operands
              << ") = " << sample.expectedValue;
}

std::ostream& operator<<(
    std::ostream& os, const OperationTestSampleConditional& sample) {
    return os << sample.condition << " ? (" << sample.thenBranch << ") : ("
              << sample.elseBranch << ") = " << sample.expectedValue;
}

std::ostream& operator<<(
    std::ostream& os, const OperationTestSampleMapping& sample) {
    return os << sample.operation << "[" << sample.operands
              << "] : " << sample.value << " = " << sample.expectedValue;
}

OperationTestSampleString stringOperations[] = {
    {"s+", R"({ "type": "value", "interface": "str1", "index": 1 },
              { "type": "value", "interface": "int", "index": 2 })",
     "foo63"},
    {"s+", R"("This ", "is ", "a ", "string.")", "This is a string."},
};

OperationTestSampleNumber numericalOperations[] = {
    {"+", R"({ "type": "value", "interface": "int" }, 111)", 234},
    {"+", R"({ "type": "value", "interface": "int", "index": 1 },
              { "type": "value", "interface": "int", "index": 4 },
              { "type": "value", "interface": "float", "index": 3 })",
     1.45},

    {"-", R"({ "type": "value", "interface": "float", "index": 1 },
              { "type": "value", "interface": "int", "index": 2 })",
     -61.8},
    {"-", R"(50, 44, -3.2)", 9.2},

    {"*", R"(3, -6)", -18},
    {"*", R"("21", "-22.23")", -466.83},
    {"*", R"(2, 4, 1, 3)", 24},
    {"*", R"(4, 76, 0, 3, 1)", 0},

    {"/", R"(15, 2)", 7.5},
    {"/", R"(64, 4, -2)", -8},
    {"/", R"(-1, -1, 2)", 0.5},
    {"/", R"(0, 2, 5, 7, -1.2)", 0},
};

OperationTestSampleString comparisons[] = {
    {"=", R"(2, 2)", "1"},
    {"=", R"("36", 36)", "1"},
    {"=", R"("42", "0000042")", "1"},
    {"=", R"(5, 5, 5, 5, 5)", "1"},
    {"=", R"(3, 5)", "0"},
    {"=", R"(12, -12)", "0"},
    {"=", R"(3, 3, 2)", "0"},

    {"!=", R"(2, 2)", "0"},
    {"!=", R"("36", 36)", "0"},
    {"!=", R"("42", "0000042")", "0"},
    {"!=", R"(3, 5)", "1"},
    {"!=", R"(12, -12)", "1"},

    {"<", R"(8, 30)", "1"},
    {"<", R"(-30, 4)", "1"},
    {"<", R"(3.4, 7.5)", "1"},
    {"<", R"(3, 4, 5, 6)", "1"},
    {"<", R"(34, 9)", "0"},
    {"<", R"(1, 1)", "0"},
    {"<", R"(3, -5)", "0"},
    {"<", R"(3, 2, 1)", "0"},
    {"<", R"(1, 2, 3, 2, 5)", "0"},

    {">", R"(8, 30)", "0"},
    {">", R"(-30, 4)", "0"},
    {">", R"(3.4, 7.5)", "0"},
    {">", R"(1, 2, 3, 4)", "0"},
    {">", R"(3, 2, 3)", "0"},
    {">", R"(34, 9)", "1"},
    {">", R"(1, 1)", "0"},
    {">", R"(3, -5)", "1"},
    {">", R"(3, 2, 1)", "1"},

    {"<=", R"(8, 30)", "1"},
    {"<=", R"(-30, 4)", "1"},
    {"<=", R"(3.4, 7.5)", "1"},
    {"<=", R"(3, 4, 5, 6)", "1"},
    {"<=", R"(34, 9)", "0"},
    {"<=", R"(1, 1)", "1"},
    {"<=", R"(3, -5)", "0"},
    {"<=", R"(1, 2, 3, 2, 5)", "0"},
    {"<=", R"(1, 2, 2, 4)", "1"},

    {">=", R"(8, 30)", "0"},
    {">=", R"(-30, 4)", "0"},
    {">=", R"(3.4, 7.5)", "0"},
    {">=", R"(3, 4, 5, 6)", "0"},
    {">=", R"(34, 9)", "1"},
    {">=", R"(1, 1)", "1"},
    {">=", R"(3, -5)", "1"},
    {">=", R"(4, 4, 4, 4)", "1"},
    {">=", R"(3, 2, 1.8)", "1"},

    {"s=", R"({ "type": "value", "interface": "str1", "index": 1 }, "foo")",
     "1"},
    {"s=", R"({ "type": "value", "interface": "str1", "index": 2 }, "foo")",
     "0"},
    {"s=", R"("bar", "bar")", "1"},
    {"s=", R"("", "")", "1"},
    {"s=", R"(0, "0")", "1"},
    {"s=", R"("xx", "xx", "xx")", "1"},
    {"s=", R"("foo", "bar")", "0"},
    {"s=", R"("foo", "fo")", "0"},
    {"s=", R"("xxx", "xxx", "xx")", "0"},

    {"s!=", R"({ "type": "value", "interface": "str1", "index": 1 }, "foo")",
     "0"},
    {"s!=", R"({ "type": "value", "interface": "str1", "index": 2 }, "foo")",
     "1"},
    {"s!=", R"("bar", "bar")", "0"},
    {"s!=", R"("", "")", "0"},
    {"s!=", R"(0, "0")", "0"},
    {"s!=", R"("foo", "bar")", "1"},
    {"s!=", R"("foo", "fo")", "1"},

    {"s<", R"("foo", "goo")", "1"},
    {"s<", R"("foo", "foobar")", "1"},
    {"s<", R"("141", "23")", "1"},
    {"s<", R"("asdasd", "sdf")", "1"},
    {"s<", R"("asd", "sdfsdf")", "1"},
    {"s<", R"("abc", "bbc", "bcd")", "1"},
    {"s<", R"("foo", "bar")", "0"},
    {"s<", R"("foobar", "bar")", "0"},
    {"s<", R"("foo", "barfoo")", "0"},
    {"s<", R"("foo", "foo")", "0"},
    {"s<", R"("abc", "cbc", "bcd")", "0"},

    {"s>", R"("foo", "goo")", "0"},
    {"s>", R"("foo", "foobar")", "0"},
    {"s>", R"("141", "23")", "0"},
    {"s>", R"("asdasd", "sdf")", "0"},
    {"s>", R"("asd", "sdfsdf")", "0"},
    {"s>", R"("ddd", "ccc", "bbb")", "1"},
    {"s>", R"("foo", "bar")", "1"},
    {"s>", R"("foobar", "bar")", "1"},
    {"s>", R"("foo", "barfoo")", "1"},
    {"s>", R"("foo", "foo")", "0"},
    {"s>", R"("abc", "cbc", "bcd")", "0"},

    {"s<=", R"("foo", "goo")", "1"},
    {"s<=", R"("foo", "foobar")", "1"},
    {"s<=", R"("141", "23")", "1"},
    {"s<=", R"("asdasd", "sdf")", "1"},
    {"s<=", R"("asd", "sdfsdf")", "1"},
    {"s<=", R"("abc", "bbc", "bcd")", "1"},
    {"s<=", R"("foo", "bar")", "0"},
    {"s<=", R"("foobar", "bar")", "0"},
    {"s<=", R"("foo", "barfoo")", "0"},
    {"s<=", R"("foo", "foo")", "1"},
    {"s<=", R"("abc", "cbc", "bcd")", "0"},

    {"s>=", R"("foo", "goo")", "0"},
    {"s>=", R"("foo", "foobar")", "0"},
    {"s>=", R"("141", "23")", "0"},
    {"s>=", R"("asdasd", "sdf")", "0"},
    {"s>=", R"("asd", "sdfsdf")", "0"},
    {"s>=", R"("ddd", "ccc", "bbb")", "1"},
    {"s>=", R"("foo", "bar")", "1"},
    {"s>=", R"("foobar", "bar")", "1"},
    {"s>=", R"("foo", "barfoo")", "1"},
    {"s>=", R"("foo", "foo")", "1"},
    {"s>=", R"("abc", "cbc", "bcd")", "0"},

    {"&", "0, 0", "0"},
    {"&", "1, 0", "0"},
    {"&", "0, 1", "0"},
    {"&", "1, 1", "1"},
    {"&", "1, 0, 1, 1", "0"},
    {"&", "1, 1, 1, 1", "1"},

    {"|", "0, 0", "0"},
    {"|", "1, 0", "1"},
    {"|", "0, 1", "1"},
    {"|", "1, 1", "1"},
    {"|", "1, 0, 0, 1", "1"},
    {"|", "0, 0, 0, 0", "0"},
};

OperationTestSampleString unaryOperations[] = {
    {"!", "0", "1"},
    {"!", "1", "0"},
};

OperationTestSampleMapping mappingOperations[] = {
    {"map", R"(
            {"min": 0, "max": 30, "value": "foo"},
            {"min": 40, "max": 60, "value": "bar"})",
     "6", "foo"},
    {"map", R"(
            {"min": 40, "max": 50, "value": "foo"},
            {"min": 0, "max": 10, "value": "bar"})",
     "6", "bar"},
    {"map", R"(
            {"min": 0, "max": 30, "value": "foo"},
            {"min": 40, "max": 60, "value": "bar"})",
     "50", "bar"},
    {"map", R"(
            {"min": 0, "max": 30, "value": "foo"},
            {"min": 40, "max": 60, "value": "bar"})",
     "-1", ""},
    {"map", R"(
            {"min": 0, "max": 30, "value": "foo"},
            {"min": 40, "max": 60, "value": "bar"})",
     "120", ""},
    {"map", R"(
            {"min": 0, "max": 30, "value": "foo"},
            {"min": 40, "max": 60, "value": "bar"})",
     "60", ""},
    {"map", R"(
            {"min": 0, "max": 30, "value": "foo"},
            {"min": 40, "max": 60, "value": "bar"})",
     "30", ""},
    {"map", R"(
            {"min": 0, "max": 30, "value": "foo"},
            {"min": 20, "max": 60, "value": "bar"})",
     "15", "foo"},
    {"map", R"(
            {"min": 0, "max": 30, "value": "foo"},
            {"min": 30, "max": 60, "value": "bar"},
            {"min": 60, "max": 80, "value": "baz"})",
     "60", "baz"},

    {"smap", R"(
            {"min": 40, "max": 90, "value": "foo"},
            {"min": 0, "max": 10, "value": "bar"})",
     "6", "foo"},
    {"smap", R"(
            {"min": "bar", "max": "foo", "value": "first"},
            {"min": "foo", "max": "widget", "value": "second"})",
     "asd", ""},
    {"smap", R"(
            {"min": "bar", "max": "foo", "value": "first"},
            {"min": "foo", "max": "widget", "value": "second"})",
     "ert", "first"},
    {"smap", R"(
            {"min": "bar", "max": "foo", "value": "first"},
            {"min": "foo", "max": "widget", "value": "second"})",
     "foo", "second"},
    {"smap", R"(
            {"min": "bar", "max": "foo", "value": "first"},
            {"min": "foo", "max": "widget", "value": "second"})",
     "vbvbvb", "second"},
    {"smap", R"(
            {"min": "bar", "max": "foo", "value": "first"},
            {"min": "foo", "max": "widget", "value": "second"})",
     "xxx", ""},
    {"smap", R"(
            {"min": "foo", "max": "widget", "value": "second"},
            {"min": "bar", "max": "foo", "value": "first"})",
     "vbvbvb", "second"},
    {"smap", R"(
            {"min": "foo", "max": "widget", "value": "second"},
            {"min": "bar", "max": "foo", "value": "first"})",
     "ert", "first"},
};

OperationTestSampleConditional conditionalOperations[] = {
    {R"({"type": "=", "ops": [2, 2]})", R"({"type": "value", "index": 1})",
     R"({"type": "value", "index": 2})", "asd"},
    {R"({"type": "=", "ops": [2, 0]})", R"({"type": "value", "index": 1})",
     R"({"type": "value", "index": 2})", "fgh"},
};

using OperationParserTestParam = std::variant<
    OperationTestSampleString, OperationTestSampleNumber,
    OperationTestSampleConditional, OperationTestSampleMapping>;

template <typename T, std::size_t N>
std::vector<OperationParserTestParam> toParamVector(const T (&arr)[N]) {
    std::vector<OperationParserTestParam> result;
    result.reserve(N);
    for (const auto& item : arr) {
        result.emplace_back(item);
    }
    return result;
}

struct OperationParserTest
    : public EspTestBase,
      public testing::WithParamInterface<OperationParserTestParam> {
    static std::vector<std::unique_ptr<InterfaceConfig>> createInterfaces() {
        std::vector<std::unique_ptr<InterfaceConfig>> result;
        result.emplace_back(std::make_unique<InterfaceConfig>());
        result.back()->name = "str1";
        result.back()->storedValue = {"foo", "bar", "foobar"};

        result.emplace_back(std::make_unique<InterfaceConfig>());
        result.back()->name = "str2";
        result.back()->storedValue = {"asd", "fgh", "jkl"};

        result.emplace_back(std::make_unique<InterfaceConfig>());
        result.back()->name = "int";
        result.back()->storedValue = {"123", "63", "0", "-110"};

        result.emplace_back(std::make_unique<InterfaceConfig>());
        result.back()->name = "float";
        result.back()->storedValue = {"1.2", "0.0", "-11.55"};

        return result;
    }

    std::unique_ptr<operation::Operation> parse(
        const std::string& json, const char* fieldName = "result",
        const char* templateName = "template") {
        auto& content = this->buffer.parseObject(json);
        return this->parser.parse(content, fieldName, templateName);
    }

    DynamicJsonBuffer buffer{512};
    std::vector<std::unique_ptr<InterfaceConfig>> interfaces =
        createInterfaces();
    operation::Parser parser{this->interfaces, this->interfaces[1].get()};
};

TEST_F(OperationParserTest, ParseConstantString) {
    std::string json = R"({"result": "test value"})";
    auto operation = parse(json);
    EXPECT_EQ(operation->evaluate(), "test value");
}

TEST_F(OperationParserTest, ParseConstantInt) {
    std::string json = R"({"result": 42})";
    auto operation = parse(json);
    EXPECT_EQ(operation->evaluate(), "42");
}

TEST_F(OperationParserTest, ParseConstantFloat) {
    std::string json = R"({"result": 2.4})";
    auto operation = parse(json);
    EXPECT_NEAR(std::atof(operation->evaluate().c_str()), 2.4, 1e-6);
}

TEST_F(OperationParserTest, ParseTemplate) {
    std::string json = R"({"template": "%1 %2 %3"})";
    auto operation = parse(json);
    EXPECT_EQ(operation->evaluate(), "asd fgh jkl");
}

TEST_F(OperationParserTest, ParseValue) {
    std::string json = R"({
        "result": {
            "type": "value",
            "interface": "str1",
            "index": 2
        }
    })";
    auto operation = parse(json);
    EXPECT_EQ(operation->evaluate(), "bar");
}

TEST_F(OperationParserTest, ParseValueDefaultIndex) {
    std::string json = R"({
        "result": {
            "type": "value",
            "interface": "str1"
        }
    })";
    auto operation = parse(json);
    EXPECT_EQ(operation->evaluate(), "foo");
}

TEST_F(OperationParserTest, ParseValueTemplate) {
    std::string json = R"({
        "result": {
            "type": "value",
            "interface": "str1",
            "template": "%1 %2 %3"
        }
    })";
    auto operation = parse(json);
    EXPECT_EQ(operation->evaluate(), "foo bar foobar");
}

TEST_F(OperationParserTest, ParseValueDefaultInterface) {
    std::string json = R"({
        "result": {
            "type": "value",
            "index": 3
        }
    })";
    auto operation = parse(json);
    EXPECT_EQ(operation->evaluate(), "jkl");
}

TEST_P(OperationParserTest, OperationTestWithStringResult) {
    const auto& sample = std::get<OperationTestSampleString>(GetParam());
    std::string json = R"({
        "result": {
            "type": ")" +
                       sample.operation + R"(",
            "ops": [)" +
                       sample.operands + R"(]
        }
    })";
    auto operation = parse(json);
    EXPECT_EQ(operation->evaluate(), sample.expectedValue);
}

INSTANTIATE_TEST_SUITE_P(
    Ops, OperationParserTest,
    testing::ValuesIn(toParamVector(stringOperations)));
INSTANTIATE_TEST_SUITE_P(
    Ops, OperationParserTest, testing::ValuesIn(toParamVector(comparisons)));

TEST_P(OperationParserTest, OperationTestWithNumericalResult) {
    const auto& sample = std::get<OperationTestSampleNumber>(GetParam());
    std::string json = R"({
        "result": {
            "type": ")" +
                       sample.operation + R"(",
            "ops": [)" +
                       sample.operands + R"(]
        }
    })";
    auto operation = parse(json);
    EXPECT_NEAR(
        std::atof(operation->evaluate().c_str()), sample.expectedValue, 1e-6);
}

INSTANTIATE_TEST_SUITE_P(
    Ops, OperationParserTest,
    testing::ValuesIn(toParamVector(numericalOperations)));

TEST_P(OperationParserTest, UnaryOperationTest) {
    const auto& sample = std::get<OperationTestSampleString>(GetParam());
    std::string json = R"({
        "result": {
            "type": ")" +
                       sample.operation + R"(",
            "op": )" + sample.operands +
                       R"(
        }
    })";
    auto operation = parse(json);
    EXPECT_EQ(operation->evaluate(), sample.expectedValue);
}

INSTANTIATE_TEST_SUITE_P(
    Ops, OperationParserTest,
    testing::ValuesIn(toParamVector(unaryOperations)));

TEST_P(OperationParserTest, ConditionalTest) {
    const auto& sample = std::get<OperationTestSampleConditional>(GetParam());
    std::string json = R"({
        "result": {
            "type": "if",
            "cond": )" +
                       sample.condition + R"(,
            "then": )" +
                       sample.thenBranch + R"(,
            "else": )" +
                       sample.elseBranch + R"(
        }
    })";
    auto operation = parse(json);
    EXPECT_EQ(operation->evaluate(), sample.expectedValue);
}

INSTANTIATE_TEST_SUITE_P(
    Ops, OperationParserTest,
    testing::ValuesIn(toParamVector(conditionalOperations)));

TEST_P(OperationParserTest, MappingTest) {
    const auto& sample = std::get<OperationTestSampleMapping>(GetParam());
    std::string json = R"({
        "result": {
            "type": ")" +
                       sample.operation + R"(",
            "ops": [)" +
                       sample.operands + R"(],
            "value": )" +
                       sample.value + R"(
        }
    })";
    auto operation = parse(json);
    EXPECT_EQ(operation->evaluate(), sample.expectedValue);
}

INSTANTIATE_TEST_SUITE_P(
    Ops, OperationParserTest,
    testing::ValuesIn(toParamVector(mappingOperations)));

TEST_F(OperationParserTest, ComplexOperation) {
    std::string json = R"({
        "result": {
            "type": "=",
            "ops": [
                {
                    "type": "+",
                    "ops": [
                        {
                            "type": "*",
                            "ops": [
                                {
                                    "type": "value",
                                    "interface": "int",
                                    "index": 1
                                },
                                {
                                    "type": "value",
                                    "interface": "float",
                                    "index": 1
                                }
                            ]
                        },
                        {
                            "type": "*",
                            "ops": [3, 2, 2]
                        },
                        -1000
                    ]
                },
                -840.4
            ]
        }
    })";
    auto operation = parse(json);
    EXPECT_EQ(operation->evaluate(), "1");
}

TEST_F(OperationParserTest, DifferentFieldName) {
    std::string json = R"({
        "foobar": "barbar"
    })";
    auto operation = parse(json, "foobar");
    EXPECT_EQ(operation->evaluate(), "barbar");
}

TEST_F(OperationParserTest, DifferentTemplateName) {
    std::string json = R"({
        "foobar": "%1"
    })";
    auto operation = parse(json, "result", "foobar");
    EXPECT_EQ(operation->evaluate(), "asd");
}

TEST_F(OperationParserTest, ValueCondition) {
    std::string json = R"({
        "result": "foobar",
        "value": "asd"
    })";
    auto operation = parse(json);
    EXPECT_EQ(operation->evaluate(), "foobar");
}

TEST_F(OperationParserTest, ValueConditionDoesNotHold) {
    std::string json = R"({
        "result": "foobar",
        "value": "asdf"
    })";
    auto operation = parse(json);
    EXPECT_EQ(operation->evaluate(), "");
}
