#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "common/SignedEnvelope.hpp"

// Tests for the signed-envelope decoder. Expected data bytes are
// hardcoded golden vectors asserted exactly; the signature fixture decodes to
// 64 identical 0xA5 bytes. Limits: command <= 512 bytes, data <= 256 bytes,
// signature exactly 64 bytes (= 88 base64 chars).

namespace {

// Base64 of 64 bytes 0xA5: raw big-endian r||s shaped ECDSA fixture (88 chars).
const char* const signature64Base64 =
    "paWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWl"
    "paWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpQ==";

// Base64 of 63 bytes 0xA5 (84 chars, no padding).
const char* const signature63Base64 =
    "paWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaW"
    "lpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWl";

// Base64 of 65 bytes 0xA5 (88 chars).
const char* const signature65Base64 =
    "paWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWl"
    "paWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaU=";

// Base64 of bytes 0..255, the maximum allowed payload (344 chars).
const char* const maxDataBase64 =
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8"
    "gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0"
    "BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fY"
    "GFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+A"
    "gYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6C"
    "hoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wM"
    "HCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4"
    "eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/w==";

// Base64 of bytes 0..255 followed by 0x42, one byte over the limit.
const char* const oversizeDataBase64 =
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8"
    "gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0"
    "BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fY"
    "GFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+A"
    "gYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6C"
    "hoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wM"
    "HCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4"
    "eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/0I=";

std::string signature64() {
    return signature64Base64;
}

std::string envelope(const std::string& data, const std::string& signature) {
    return "{\"data\":\"" + data + "\",\"signature\":\"" + signature + "\"}";
}

// Builds an envelope whose data field carries a raw JSON value instead of a
// quoted string, for testing non-string field values.
std::string envelopeRawData(const std::string& dataValue) {
    return "{\"data\":" + dataValue + ",\"signature\":\"" + signature64() +
           "\"}";
}

// Success must decode the exact data bytes and the 64 x 0xA5 signature,
// fully replacing the previous output contents.
void expectSuccess(
    const std::string& command, const std::vector<uint8_t>& expectedData) {
    SignedEnvelope output;
    output.data.assign(16, 0xEE);
    output.signature.fill(0xEE);

    EXPECT_EQ(decodeSignedEnvelope(command, output), EnvelopeError::none);
    EXPECT_EQ(output.data, expectedData);

    std::array<uint8_t, 64> expectedSignature;
    expectedSignature.fill(0xA5);
    EXPECT_EQ(output.signature, expectedSignature);
}

// Failure must return the mapped error and reset the output: data emptied
// and signature zeroed.
void expectError(const std::string& command, EnvelopeError expected) {
    SignedEnvelope output;
    output.data.assign(16, 0xEE);
    output.signature.fill(0xEE);

    EXPECT_EQ(decodeSignedEnvelope(command, output), expected);
    EXPECT_TRUE(output.data.empty());

    const std::array<uint8_t, 64> zeroSignature{};
    EXPECT_EQ(output.signature, zeroSignature);
}

std::vector<uint8_t> rangeData() {
    std::vector<uint8_t> data;
    for (int i = 0; i < 256; ++i) {
        data.push_back(static_cast<uint8_t>(i));
    }
    return data;
}

}  // namespace

TEST(SignedEnvelopeTest, AcceptsEmptyData) {
    expectSuccess(envelope("", signature64()), std::vector<uint8_t>());
}

TEST(SignedEnvelopeTest, AcceptsBinaryDataWithNulAndHighBits) {
    expectSuccess(
        envelope("AP8B", signature64()),
        std::vector<uint8_t>{0x00, 0xFF, 0x01});
}

TEST(SignedEnvelopeTest, AcceptsMaximumData) {
    const std::string command = envelope(maxDataBase64, signature64());
    ASSERT_EQ(command.size(), 458u);
    expectSuccess(command, rangeData());
}

TEST(SignedEnvelopeTest, AcceptsFieldsInAnyOrder) {
    const std::string command =
        "{\"signature\":\"" + signature64() + "\",\"data\":\"AP8B\"}";
    expectSuccess(command, std::vector<uint8_t>{0x00, 0xFF, 0x01});
}

TEST(SignedEnvelopeTest, AcceptsWhitespaceBetweenTokens) {
    const std::string command =
        " { \"data\" : \"AP8B\" , \"signature\" : \"" + signature64() + "\" } ";
    expectSuccess(command, std::vector<uint8_t>{0x00, 0xFF, 0x01});
}

TEST(SignedEnvelopeTest, AcceptsExactly512ByteCommand) {
    // Pad a valid envelope with spaces to exactly the 512 byte limit.
    const std::string minimal = envelope("", signature64());
    ASSERT_EQ(minimal.size(), 114u);
    const std::string command = minimal.substr(0, 1) +
                                std::string(512 - minimal.size(), ' ') +
                                minimal.substr(1);

    ASSERT_EQ(command.size(), 512u);
    expectSuccess(command, std::vector<uint8_t>());
}

TEST(SignedEnvelopeTest, RejectsCommandLargerThan512) {
    const std::string command = envelope("", signature64());
    ASSERT_EQ(command.size(), 114u);
    const std::string padded = command.substr(0, 1) +
                               std::string(513 - command.size(), ' ') +
                               command.substr(1);
    ASSERT_EQ(padded.size(), 513u);
    expectError(padded, EnvelopeError::commandTooLarge);

    // Rejected before parsing, regardless of content.
    expectError(std::string(513, 'x'), EnvelopeError::commandTooLarge);
}

TEST(SignedEnvelopeTest, RejectsDataAbove256Bytes) {
    const std::string command = envelope(oversizeDataBase64, signature64());
    ASSERT_EQ(command.size(), 458u);
    expectError(command, EnvelopeError::dataTooLarge);
}

TEST(SignedEnvelopeTest, RejectsSignatureShorterThan64Bytes) {
    expectError(
        envelope("AP8B", signature63Base64),
        EnvelopeError::invalidSignatureLength);
}

TEST(SignedEnvelopeTest, RejectsSignatureLongerThan64Bytes) {
    expectError(
        envelope("AP8B", signature65Base64),
        EnvelopeError::invalidSignatureLength);
}

TEST(SignedEnvelopeTest, RejectsEmptySignature) {
    expectError(envelope("AP8B", ""), EnvelopeError::invalidSignatureLength);
    expectError(envelope("", ""), EnvelopeError::invalidSignatureLength);
}

TEST(SignedEnvelopeTest, RejectsInvalidSignatureBase64) {
    std::string invalid = signature64();
    invalid.replace(0, 4, "!!!!");
    expectError(envelope("AP8B", invalid), EnvelopeError::invalidBase64);
}

TEST(SignedEnvelopeTest, DuplicateFieldsUseLastValues) {
    const std::string duplicateData =
        "{\"data\":\"AP8B\",\"data\":\"AQ==\",\"signature\":\"" +
        signature64() + "\"}";
    expectSuccess(duplicateData, std::vector<uint8_t>{0x01});

    const std::string duplicateSignature =
        "{\"data\":\"AP8B\",\"signature\":\"" + std::string(signature63Base64) +
        "\",\"signature\":\"" + signature64() + "\"}";
    expectSuccess(duplicateSignature, std::vector<uint8_t>{0x00, 0xFF, 0x01});
}

TEST(SignedEnvelopeTest, RejectsUnknownField) {
    const std::string command = "{\"data\":\"AP8B\",\"signature\":\"" +
                                signature64() + "\",\"extra\":1}";
    expectError(command, EnvelopeError::invalidFields);
}

TEST(SignedEnvelopeTest, RejectsMissingFields) {
    const std::string missingData = "{\"signature\":\"" + signature64() + "\"}";
    expectError(missingData, EnvelopeError::invalidFields);

    const std::string missingSignature = "{\"data\":\"AP8B\"}";
    expectError(missingSignature, EnvelopeError::invalidFields);
}

TEST(SignedEnvelopeTest, RejectsNonObjectRoot) {
    expectError("[]", EnvelopeError::invalidFields);
    expectError("42", EnvelopeError::invalidFields);
    expectError("\"data\"", EnvelopeError::invalidFields);
    expectError("true", EnvelopeError::invalidFields);
    expectError("null", EnvelopeError::invalidFields);
    expectError("{}", EnvelopeError::invalidFields);
}

TEST(SignedEnvelopeTest, RejectsNonStringFields) {
    expectError(envelopeRawData("1"), EnvelopeError::invalidFields);
    expectError(envelopeRawData("1.5"), EnvelopeError::invalidFields);
    expectError(envelopeRawData("true"), EnvelopeError::invalidFields);
    expectError(envelopeRawData("null"), EnvelopeError::invalidFields);
    expectError(envelopeRawData("[65]"), EnvelopeError::invalidFields);
    expectError(
        envelopeRawData("{\"nested\":1}"), EnvelopeError::invalidFields);
    expectError(
        "{\"data\":\"AP8B\",\"signature\":64}", EnvelopeError::invalidFields);
}

TEST(SignedEnvelopeTest, RejectsTruncatedJson) {
    expectError("", EnvelopeError::invalidJson);
    expectError("{", EnvelopeError::invalidJson);
    expectError(
        envelope("AP8B", signature64()).substr(0, 6),
        EnvelopeError::invalidJson);
}

TEST(SignedEnvelopeTest, RejectsTrailingData) {
    expectError(
        envelope("AP8B", signature64()) + " x", EnvelopeError::invalidJson);
    expectError(
        envelope("AP8B", signature64()) + "}", EnvelopeError::invalidJson);
}

TEST(SignedEnvelopeTest, RejectsEmbeddedNul) {
    std::string command = envelope("AP8B", signature64());
    command.insert(command.find("signature"), 1, '\0');
    expectError(command, EnvelopeError::invalidJson);
}

TEST(SignedEnvelopeTest, RejectsEscapedStructuralTokenOutsideString) {
    const std::string validEnvelope = envelope("AP8B", signature64());
    const std::string command =
        validEnvelope.substr(0, validEnvelope.size() - 1) + "\\u007d";
    expectError(command, EnvelopeError::invalidJson);
}

TEST(SignedEnvelopeTest, RejectsNonCanonicalDataBase64) {
    // Whitespace inside the value is part of the base64 and rejected.
    expectError(envelope("AP 8B", signature64()), EnvelopeError::invalidBase64);
    expectError(envelope("QQ", signature64()), EnvelopeError::invalidBase64);
    expectError(envelope("Zm8-", signature64()), EnvelopeError::invalidBase64);
    expectError(envelope("QR==", signature64()), EnvelopeError::invalidBase64);
}

TEST(SignedEnvelopeTest, RejectsNonCanonicalDoublePaddingGroup) {
    // Regression test for a pad-bits mask bug: the non-canonical group
    // "QU==" permissively decodes to the same byte 'A' as the canonical
    // "QQ==" (U(20) has nonzero unused low four bits), so accepting it
    // would make the envelope encoding malleable.
    expectError(envelope("QU==", signature64()), EnvelopeError::invalidBase64);

    // Same defect reachable through the signature field: the final group
    // "pU==" permissively decodes to the same 0xA5 byte as the canonical
    // "pQ==", differing only in the nonzero unused bits.
    std::string nonCanonicalSignature = signature64();
    ASSERT_EQ(
        nonCanonicalSignature.substr(nonCanonicalSignature.size() - 4), "pQ==");
    nonCanonicalSignature.replace(nonCanonicalSignature.size() - 3, 1, "U");
    expectError(
        envelope("AP8B", nonCanonicalSignature), EnvelopeError::invalidBase64);
}

TEST(SignedEnvelopeTest, ResetsPreviousOutputOnFailure) {
    SignedEnvelope output;

    ASSERT_EQ(
        decodeSignedEnvelope(envelope("AP8B", signature64()), output),
        EnvelopeError::none);
    EXPECT_EQ(output.data.size(), 3u);

    EXPECT_EQ(
        decodeSignedEnvelope(envelope("Zm!v", signature64()), output),
        EnvelopeError::invalidBase64);
    EXPECT_TRUE(output.data.empty());
    const std::array<uint8_t, 64> zeroSignature{};
    EXPECT_EQ(output.signature, zeroSignature);
}
