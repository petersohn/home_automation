#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

#include "tools/JsonParsing.hpp"

namespace {

class ReadOnlyJsonInput {
public:
    explicit ReadOnlyJsonInput(std::string text) : text(std::move(text)) {}

    int read() {
        if (this->position == this->text.size()) {
            return -1;
        }
        return static_cast<unsigned char>(this->text[this->position++]);
    }

private:
    std::string text;
    std::size_t position = 0;
};

}  // namespace

TEST(JsonParsingTest, ParsesCountedInputAndAllowsTrailingWhitespace) {
    ArduinoJson::DynamicJsonBuffer buffer{128};
    const ArduinoJson::JsonVariant parsed =
        tools::parseFullJson("{value:7} \t\r\n", buffer, 8);

    ASSERT_TRUE(parsed.success());
    ASSERT_TRUE(parsed.is<ArduinoJson::JsonObject>());
    EXPECT_EQ(parsed.as<ArduinoJson::JsonObject>().get<uint32_t>("value"), 7u);
}

TEST(JsonParsingTest, ParsesStreamObjectWithWhitespaceAndDuplicateKeys) {
    ReadOnlyJsonInput input("{value:1, value:7} \t\r\n");
    ArduinoJson::DynamicJsonBuffer buffer{128};

    const ArduinoJson::JsonVariant parsed =
        tools::parseFullJson(input, buffer, 8);

    ASSERT_TRUE(parsed.success());
    ASSERT_TRUE(parsed.is<ArduinoJson::JsonObject>());
    EXPECT_EQ(parsed.as<ArduinoJson::JsonObject>().get<uint32_t>("value"), 7u);
}

TEST(JsonParsingTest, RejectsMalformedAndTruncatedStreamInput) {
    const std::array<const char*, 3> inputs = {"{", "{value:", "{value:7"};

    for (const char* text : inputs) {
        ReadOnlyJsonInput input(text);
        ArduinoJson::DynamicJsonBuffer buffer{128};
        EXPECT_FALSE(tools::parseFullJson(input, buffer, 8).success()) << text;
    }
}

TEST(JsonParsingTest, RejectsTrailingGarbageAndSecondStreamDocument) {
    const std::array<const char*, 2> inputs = {"{} trailing", "{} {}"};

    for (const char* text : inputs) {
        ReadOnlyJsonInput input(text);
        ArduinoJson::DynamicJsonBuffer buffer{128};
        EXPECT_FALSE(tools::parseFullJson(input, buffer, 8).success()) << text;
    }
}

TEST(JsonParsingTest, RejectsNulBytesBeforeWithinAndAfterStreamDocument) {
    const char beforeDocument[] = "\0{}";
    const char withinDocument[] = "{value:\"left\0right\"}";
    const char afterDocument[] = "{}\0";
    const char beforeUnreadTail[] = "{}\0tail";
    const char beforeSecondDocument[] = "{} \0{}";
    const std::array<std::string, 5> inputs = {
        std::string(beforeDocument, sizeof(beforeDocument) - 1),
        std::string(withinDocument, sizeof(withinDocument) - 1),
        std::string(afterDocument, sizeof(afterDocument) - 1),
        std::string(beforeUnreadTail, sizeof(beforeUnreadTail) - 1),
        std::string(beforeSecondDocument, sizeof(beforeSecondDocument) - 1)};

    for (const std::string& text : inputs) {
        ReadOnlyJsonInput input(text);
        ArduinoJson::DynamicJsonBuffer buffer{128};
        EXPECT_FALSE(tools::parseFullJson(input, buffer, 8).success());
    }
}

TEST(JsonParsingTest, EnforcesExplicitNestingLimitForStreamInput) {
    ReadOnlyJsonInput withinLimitInput("{outer:{inner:1}}");
    ArduinoJson::DynamicJsonBuffer withinLimitBuffer{128};
    EXPECT_TRUE(
        tools::parseFullJson(withinLimitInput, withinLimitBuffer, 2).success());

    ReadOnlyJsonInput beyondLimitInput("{outer:{inner:1}}");
    ArduinoJson::DynamicJsonBuffer beyondLimitBuffer{128};
    EXPECT_FALSE(
        tools::parseFullJson(beyondLimitInput, beyondLimitBuffer, 1).success());
}

TEST(JsonParsingTest, CopiesStreamStringsIntoJsonBuffer) {
    ArduinoJson::DynamicJsonBuffer buffer{128};
    ArduinoJson::JsonVariant parsed;
    {
        ReadOnlyJsonInput input("{key:\"value\"}");
        parsed = tools::parseFullJson(input, buffer, 8);
    }

    ASSERT_TRUE(parsed.success());
    const char* value =
        parsed.as<ArduinoJson::JsonObject>().get<const char*>("key");
    ASSERT_NE(value, nullptr);
    EXPECT_STREQ(value, "value");
}

TEST(JsonParsingTest, ParsesLargeInputThroughReadOnlyStreamInterface) {
    const std::size_t payloadSize = 4096;
    std::string text = "{payload:\"";
    text.append(payloadSize, 'x');
    text += "\"}";
    ReadOnlyJsonInput input(std::move(text));
    ArduinoJson::DynamicJsonBuffer buffer{8192};

    const ArduinoJson::JsonVariant parsed =
        tools::parseFullJson(input, buffer, 8);

    ASSERT_TRUE(parsed.success());
    const char* payload =
        parsed.as<ArduinoJson::JsonObject>().get<const char*>("payload");
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(std::strlen(payload), payloadSize);
    EXPECT_EQ(payload[payloadSize - 1], 'x');
}

TEST(JsonParsingTest, RejectsEmbeddedNulAndTrailingGarbage) {
    const char textWithNul[] = "{value:7}\0{}";
    ArduinoJson::DynamicJsonBuffer nulBuffer{128};
    EXPECT_FALSE(
        tools::parseFullJson(
            std::string(textWithNul, sizeof(textWithNul) - 1), nulBuffer, 8)
            .success());

    ArduinoJson::DynamicJsonBuffer garbageBuffer{128};
    EXPECT_FALSE(
        tools::parseFullJson("{value:7} trailing", garbageBuffer, 8).success());
}

TEST(JsonParsingTest, EnforcesNestingLimit) {
    ArduinoJson::DynamicJsonBuffer withinLimitBuffer{128};
    EXPECT_TRUE(tools::parseFullJson("[[0]]", withinLimitBuffer, 2).success());

    ArduinoJson::DynamicJsonBuffer beyondLimitBuffer{128};
    EXPECT_FALSE(tools::parseFullJson("[[0]]", beyondLimitBuffer, 1).success());
}

TEST(JsonParsingTest, ParsesCheckedUint32IncludingSignedZeroSyntax) {
    const std::array<std::pair<const char*, uint32_t>, 4> validInputs = {{
        {"0", 0u},
        {"+0", 0u},
        {"-0", 0u},
        {"4294967295", UINT32_MAX},
    }};

    for (const auto& input : validInputs) {
        ArduinoJson::DynamicJsonBuffer buffer{128};
        const ArduinoJson::JsonVariant parsed =
            tools::parseFullJson(input.first, buffer, 1);
        ASSERT_TRUE(parsed.success()) << input.first;

        uint32_t result = 91;
        ASSERT_TRUE(tools::parseUint32(parsed, result)) << input.first;
        EXPECT_EQ(result, input.second);
    }
}

TEST(JsonParsingTest, RejectsInvalidUint32SyntaxTypesAndOverflow) {
    const std::array<const char*, 7> invalidInputs = {
        "-1", "4294967296", "1.0", "1e0", "1x", "true", "\"1\""};

    for (const char* input : invalidInputs) {
        ArduinoJson::DynamicJsonBuffer buffer{128};
        const ArduinoJson::JsonVariant parsed =
            tools::parseFullJson(input, buffer, 1);
        ASSERT_TRUE(parsed.success()) << input;

        uint32_t result = 91;
        EXPECT_FALSE(tools::parseUint32(parsed, result)) << input;
        EXPECT_EQ(result, 91u);
    }
}
