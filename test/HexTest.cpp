#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "tools/Hex.hpp"

TEST(HexTest, DecodesExactLengthLowercaseHexIntoFixedArray) {
    std::array<uint8_t, 4> decoded{};

    ASSERT_TRUE(tools::decodeLowercaseHex("00a1feff", decoded));
    EXPECT_EQ(decoded, (std::array<uint8_t, 4>{0x00, 0xA1, 0xFE, 0xFF}));
}

TEST(HexTest, RejectsWrongLengthUppercaseAndInvalidCharacters) {
    std::array<uint8_t, 4> decoded{};

    EXPECT_FALSE(tools::decodeLowercaseHex("00a1fe", decoded));
    EXPECT_FALSE(tools::decodeLowercaseHex("00a1feff00", decoded));
    EXPECT_FALSE(tools::decodeLowercaseHex("00A1feff", decoded));
    EXPECT_FALSE(tools::decodeLowercaseHex("00a1fe0g", decoded));
}
