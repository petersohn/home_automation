#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "tools/StrictBase64.hpp"

namespace {

std::vector<uint8_t> bytesOf(const std::string& chars) {
    return std::vector<uint8_t>(chars.begin(), chars.end());
}

void expectDecode(
    const std::string& input, const std::vector<uint8_t>& expected) {
    std::vector<uint8_t> output(16, 0xEE);

    ASSERT_TRUE(tools::decodeCanonicalBase64(input, expected.size(), output));
    EXPECT_EQ(output, expected);
}

void expectReject(const std::string& input) {
    std::vector<uint8_t> output(8, 0xEE);

    EXPECT_FALSE(tools::decodeCanonicalBase64(input, 4096, output));
    EXPECT_TRUE(output.empty());
}

}  // namespace

TEST(StrictBase64Test, DecodesEmptyInput) {
    expectDecode("", std::vector<uint8_t>());
}

TEST(StrictBase64Test, DecodesRfc4648TestVectors) {
    expectDecode("Zg==", bytesOf("f"));
    expectDecode("Zm8=", bytesOf("fo"));
    expectDecode("Zm9v", bytesOf("foo"));
    expectDecode("Zm9vYg==", bytesOf("foob"));
    expectDecode("Zm9vYmE=", bytesOf("fooba"));
    expectDecode("Zm9vYmFy", bytesOf("foobar"));
}

TEST(StrictBase64Test, DecodesBinaryDataExactly) {
    expectDecode("AA==", std::vector<uint8_t>{0x00});
    expectDecode("/w==", std::vector<uint8_t>{0xFF});
    expectDecode("AAEC", std::vector<uint8_t>{0x00, 0x01, 0x02});
    expectDecode("AP8B", std::vector<uint8_t>{0x00, 0xFF, 0x01});
    expectDecode("3q2+7w==", std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF});
    expectDecode(
        "AAECAwQFBgcICQoLDA0ODw==",
        std::vector<uint8_t>{
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
            0x0B, 0x0C, 0x0D, 0x0E, 0x0F});
}

TEST(StrictBase64Test, DecodesAllByteValues) {
    // Base64 of bytes 0..255 (one "==" padded final group).
    const std::string input =
        "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMz"
        "Q1Njc4OTo7PD"
        "0+"
        "P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3Bxcn"
        "N0dXZ3eHl6"
        "e3x9fn+"
        "AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+"
        "wsbKztLW2t7"
        "i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/"
        "g4eLj5OXm5+jp6uvs7e7v8PHy8/T1"
        "9vf4+fr7/P3+/w==";
    std::vector<uint8_t> expected;
    for (int i = 0; i < 256; ++i) {
        expected.push_back(static_cast<uint8_t>(i));
    }

    expectDecode(input, expected);
}

TEST(StrictBase64Test, AcceptsExactOutputBound) {
    std::vector<uint8_t> output;

    ASSERT_TRUE(tools::decodeCanonicalBase64("Zm9v", 3, output));
    EXPECT_EQ(output, bytesOf("foo"));

    ASSERT_TRUE(tools::decodeCanonicalBase64("Zm9vYg==", 4, output));
    EXPECT_EQ(output, bytesOf("foob"));
}

TEST(StrictBase64Test, RejectsMissingPadding) {
    expectReject("Q");
    expectReject("Zg");
    expectReject("Zg=");
    expectReject("Zm9vYg");
    expectReject("Zm9v=");
}

TEST(StrictBase64Test, RejectsExtraOrMisplacedPadding) {
    expectReject("Zg===");
    expectReject("A===");
    expectReject("=");
    expectReject("==");
    expectReject("Zm9vYg===");
    expectReject("Zg==Zm8=");
}

TEST(StrictBase64Test, RejectsNonzeroUnusedBits) {
    // QR== has nonzero low four bits.
    expectReject("QR==");
    // QUW= has nonzero low two bits.
    expectReject("QUW=");
}

TEST(StrictBase64Test, RejectsNonzeroUnusedBitsInDoublePaddingGroup) {
    // Catch checks that test only two of the four unused bits in XY== groups.
    expectReject("QU==");
    expectReject("QY==");
    expectReject("Zm9vQU==");
}

TEST(StrictBase64Test, DecodesCanonicalDoublePaddingGroup) {
    // Canonical control for the non-canonical XY== vectors above.
    expectDecode("QQ==", bytesOf("A"));
}

TEST(StrictBase64Test, RejectsWhitespace) {
    expectReject(" Zm9v");
    expectReject("Zm9v ");
    expectReject("Zm9\tv");
    expectReject("Zm9v\n");
    expectReject("Z m9v");
}

TEST(StrictBase64Test, RejectsUrlSafeAlphabet) {
    expectReject("-A==");
    expectReject("Zm8-");
    expectReject("-_-=");
    expectReject("A-_B");
}

TEST(StrictBase64Test, RejectsInvalidCharacters) {
    expectReject("Zm!v");
    expectReject("!///");
    expectReject(std::string(1, '\xFF') + "===");
    expectReject(std::string("Zm\x00v", 4));
}

TEST(StrictBase64Test, RejectsOutputBoundTooSmall) {
    std::vector<uint8_t> output(8, 0xEE);

    EXPECT_FALSE(tools::decodeCanonicalBase64("Zm9v", 2, output));
    EXPECT_TRUE(output.empty());

    EXPECT_FALSE(tools::decodeCanonicalBase64("Zm9vYg==", 3, output));
    EXPECT_TRUE(output.empty());

    // 344 'A's decode to 258 bytes, exceeding this bound.
    EXPECT_FALSE(
        tools::decodeCanonicalBase64(std::string(344, 'A'), 255, output));
    EXPECT_TRUE(output.empty());
}

TEST(StrictBase64Test, ReplacesPreviousOutputOnSuccess) {
    std::vector<uint8_t> output(32, 0xEE);

    ASSERT_TRUE(tools::decodeCanonicalBase64("Zm9v", 100, output));
    EXPECT_EQ(output, bytesOf("foo"));
}

TEST(StrictBase64Test, ClearsPreviousOutputOnFailure) {
    std::vector<uint8_t> output(32, 0xEE);

    EXPECT_FALSE(tools::decodeCanonicalBase64("Zm!v", 100, output));
    EXPECT_TRUE(output.empty());
}

TEST(StrictBase64Test, CalculatesDecodedUpperBoundFromLengthAndPadding) {
    EXPECT_EQ(tools::decodedUpperBound(""), 0u);
    EXPECT_EQ(tools::decodedUpperBound("A"), 3u);
    EXPECT_EQ(tools::decodedUpperBound("AA=="), 1u);
    EXPECT_EQ(tools::decodedUpperBound("AAA="), 2u);
    EXPECT_EQ(tools::decodedUpperBound("AAAA"), 3u);
    EXPECT_EQ(tools::decodedUpperBound("AAAAA==="), 4u);
}

TEST(StrictBase64Test, EstimatesMalformedInputSizeWithoutValidatingIt) {
    EXPECT_EQ(tools::decodedUpperBound("A==="), 1u);
    EXPECT_EQ(tools::decodedUpperBound("A="), 2u);
    EXPECT_EQ(tools::decodedUpperBound("===x"), 3u);
}
