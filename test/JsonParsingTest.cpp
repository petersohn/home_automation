#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <utility>

#include "tools/JsonParsing.hpp"

TEST(JsonParsingTest, ParsesCountedInputAndAllowsTrailingWhitespace) {
    ArduinoJson::DynamicJsonBuffer buffer{128};
    const ArduinoJson::JsonVariant parsed =
        tools::parseFullJson("{value:7} \t\r\n", buffer, 8);

    ASSERT_TRUE(parsed.success());
    ASSERT_TRUE(parsed.is<ArduinoJson::JsonObject>());
    EXPECT_EQ(parsed.as<ArduinoJson::JsonObject>().get<uint32_t>("value"), 7u);
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
