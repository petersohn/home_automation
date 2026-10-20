#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>

#include "common/StreamCommand.hpp"

namespace {

const std::string hashText(64, 'a');
const std::string signatureText =
    "paWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWl"
    "paWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpQ==";

std::string beginCommand(const std::string& fields = "\"size\":1") {
    return "{\"type\":\"begin\",\"hash\":\"" + hashText + "\"," + fields + "}";
}

std::string chunkCommand(
    const std::string& fields = "\"offset\":0,\"data\":\"AQ==\"") {
    return "{\"type\":\"chunk\",\"hash\":\"" + hashText + "\"," + fields + "}";
}

std::string finishCommand(
    const std::string& fields = "\"signature\":\"" + signatureText + "\"") {
    return "{\"type\":\"finish\",\"hash\":\"" + hashText + "\"," + fields + "}";
}

void expectDecodeError(
    const std::string& text, StreamCommandError expectedError) {
    StreamCommand output;
    output.type = StreamCommandType::finish;
    output.hash.fill(0xEE);
    output.size = 17;
    output.offset = 19;
    output.data.fill(0xEE);
    output.dataSize = 23;
    output.signature.fill(0xEE);

    EXPECT_EQ(decodeStreamCommand(text, output), expectedError);
    EXPECT_EQ(output.type, StreamCommandType::begin);
    EXPECT_EQ(output.hash, (std::array<uint8_t, 32>{}));
    EXPECT_EQ(output.size, 0u);
    EXPECT_EQ(output.offset, 0u);
    EXPECT_EQ(output.data, (std::array<uint8_t, 256>{}));
    EXPECT_EQ(output.dataSize, 0u);
    EXPECT_EQ(output.signature, (std::array<uint8_t, 64>{}));
}

}  // namespace

TEST(StreamCommandTest, DecodesExactBeginFields) {
    StreamCommand output;
    output.data.fill(0xEE);
    output.signature.fill(0xEE);

    EXPECT_EQ(
        decodeStreamCommand(beginCommand(), output), StreamCommandError::none);
    EXPECT_EQ(output.type, StreamCommandType::begin);
    EXPECT_EQ(output.hash[0], 0xAA);
    EXPECT_EQ(output.hash[31], 0xAA);
    EXPECT_EQ(output.size, 1u);
    EXPECT_EQ(output.offset, 0u);
    EXPECT_EQ(output.data, (std::array<uint8_t, 256>{}));
    EXPECT_EQ(output.dataSize, 0u);
    EXPECT_EQ(output.signature, (std::array<uint8_t, 64>{}));
}

TEST(StreamCommandTest, DecodesExactChunkFields) {
    StreamCommand output;
    ASSERT_EQ(
        decodeStreamCommand(chunkCommand(), output), StreamCommandError::none);

    EXPECT_EQ(output.type, StreamCommandType::chunk);
    EXPECT_EQ(output.hash[0], 0xAA);
    EXPECT_EQ(output.size, 0u);
    EXPECT_EQ(output.offset, 0u);
    EXPECT_EQ(output.dataSize, 1u);
    EXPECT_EQ(output.data[0], 0x01);
    EXPECT_EQ(output.data[1], 0u);
    EXPECT_EQ(output.signature, (std::array<uint8_t, 64>{}));
}

TEST(StreamCommandTest, DecodesExactFinishFields) {
    StreamCommand output;
    ASSERT_EQ(
        decodeStreamCommand(finishCommand(), output), StreamCommandError::none);

    EXPECT_EQ(output.type, StreamCommandType::finish);
    EXPECT_EQ(output.hash[0], 0xAA);
    EXPECT_EQ(output.size, 0u);
    EXPECT_EQ(output.offset, 0u);
    EXPECT_EQ(output.data, (std::array<uint8_t, 256>{}));
    EXPECT_EQ(output.dataSize, 0u);
    EXPECT_EQ(output.signature[0], 0xA5);
    EXPECT_EQ(output.signature[63], 0xA5);
}

TEST(StreamCommandTest, DecodesNativeArduinoJsonBeginSyntax) {
    const std::string command =
        "{'ty\\pe':'begin', /* internal comment */ 'ha\\sh':'" + hashText +
        "', size:7}";
    StreamCommand output;

    EXPECT_EQ(decodeStreamCommand(command, output), StreamCommandError::none);
    EXPECT_EQ(output.type, StreamCommandType::begin);
    EXPECT_EQ(output.hash[0], 0xAA);
    EXPECT_EQ(output.size, 7u);
}

TEST(StreamCommandTest, DecodesNativeArduinoJsonChunkSyntax) {
    const std::string command =
        "{type:'chunk', hash:'" + hashText +
        "', /* offset comment */ '\\offset':0, data:'AQ=='}";
    StreamCommand output;

    EXPECT_EQ(decodeStreamCommand(command, output), StreamCommandError::none);
    EXPECT_EQ(output.type, StreamCommandType::chunk);
    EXPECT_EQ(output.hash[0], 0xAA);
    EXPECT_EQ(output.offset, 0u);
    ASSERT_EQ(output.dataSize, 1u);
    EXPECT_EQ(output.data[0], 0x01);
}

TEST(StreamCommandTest, NativeDuplicateChunkOffsetUsesLastValue) {
    const std::string command = "{type:'chunk', hash:'" + hashText +
                                "', offset:2, offset:9, data:'AQ=='}";
    StreamCommand output;

    EXPECT_EQ(decodeStreamCommand(command, output), StreamCommandError::none);
    EXPECT_EQ(output.offset, 9u);
}

TEST(StreamCommandTest, RejectsMissingUnknownAndWrongTypeFields) {
    expectDecodeError(
        "{\"type\":\"begin\",\"hash\":\"" + hashText + "\"}",
        StreamCommandError::invalidFields);
    expectDecodeError(
        beginCommand("\"size\":1,\"extra\":true"),
        StreamCommandError::invalidFields);
    expectDecodeError(
        "{\"type\":\"unknown\",\"hash\":\"" + hashText + "\",\"size\":1}",
        StreamCommandError::invalidFields);
    expectDecodeError(
        chunkCommand("\"offset\":0,\"data\":\"\""),
        StreamCommandError::invalidFields);
    expectDecodeError(
        chunkCommand("\"offset\":0,\"data\":1"),
        StreamCommandError::invalidFields);
}

TEST(StreamCommandTest, EnforcesCommandAndChunkBounds) {
    const std::string payload256 = std::string(340, 'A') + "AA==";
    const std::string maxChunk =
        chunkCommand("\"offset\":4294967039,\"data\":\"" + payload256 + "\"");
    ASSERT_LE(maxChunk.size(), 512u);

    StreamCommand output;
    ASSERT_EQ(decodeStreamCommand(maxChunk, output), StreamCommandError::none);
    EXPECT_EQ(output.offset, 4294967039u);
    EXPECT_EQ(output.dataSize, 256u);
    EXPECT_EQ(output.data[255], 0u);

    const std::string payload257 = std::string(340, 'A') + "AAA=";
    expectDecodeError(
        chunkCommand("\"offset\":0,\"data\":\"" + payload257 + "\""),
        StreamCommandError::dataTooLarge);

    expectDecodeError(
        beginCommand() + std::string(513, ' '),
        StreamCommandError::commandTooLarge);
}

TEST(StreamCommandTest, RejectsEmbeddedNulTrailingJunkAndExcessiveDepth) {
    std::string embeddedNul = beginCommand();
    embeddedNul.insert(embeddedNul.find("\"size\""), 1, '\0');
    expectDecodeError(embeddedNul, StreamCommandError::invalidJson);
    expectDecodeError(beginCommand() + " x", StreamCommandError::invalidJson);
    expectDecodeError(
        beginCommand("\"size\":1,\"extra\":[[[[[[[[[]]]]]]]]]"),
        StreamCommandError::invalidJson);
}

TEST(StreamCommandTest, RejectsMalformedHashAndBase64) {
    std::string uppercaseHash = hashText;
    uppercaseHash[0] = 'A';
    expectDecodeError(
        "{\"type\":\"begin\",\"hash\":\"" + uppercaseHash + "\",\"size\":1}",
        StreamCommandError::invalidHash);
    expectDecodeError(
        "{\"type\":\"begin\",\"hash\":\"abc\",\"size\":1}",
        StreamCommandError::invalidHash);
    expectDecodeError(
        chunkCommand("\"offset\":0,\"data\":\"QQ\""),
        StreamCommandError::invalidBase64);
    expectDecodeError(
        finishCommand("\"signature\":\"AQ==\""),
        StreamCommandError::invalidSignatureLength);
}

TEST(StreamCommandTest, RejectsNonIntegerAndOutOfRangeCounts) {
    const std::array<std::string, 5> invalidCounts = {
        "false", "true", "-1", "1.0", "4294967296"};
    for (const std::string& count : invalidCounts) {
        expectDecodeError(
            beginCommand("\"size\":" + count),
            StreamCommandError::invalidFields);
        expectDecodeError(
            chunkCommand("\"offset\":" + count + ",\"data\":\"AQ==\""),
            StreamCommandError::invalidFields);
    }

    StreamCommand output;
    EXPECT_EQ(
        decodeStreamCommand(beginCommand("\"size\":4294967295"), output),
        StreamCommandError::none);
    EXPECT_EQ(output.size, 4294967295u);
    EXPECT_EQ(
        decodeStreamCommand(
            chunkCommand("\"offset\":4294967295,\"data\":\"AQ==\""), output),
        StreamCommandError::none);
    EXPECT_EQ(output.offset, 4294967295u);
}

TEST(StreamCommandTest, DuplicateLiteralSizeUsesLastValue) {
    const std::string command = "{\"type\":\"begin\",\"hash\":\"" + hashText +
                                "\",\"size\":2,\"size\":4294967295}";
    StreamCommand output;

    EXPECT_EQ(decodeStreamCommand(command, output), StreamCommandError::none);
    EXPECT_EQ(output.size, 4294967295u);
}

TEST(StreamCommandTest, FailureAfterSuccessResetsWholeOutput) {
    StreamCommand output;
    ASSERT_EQ(
        decodeStreamCommand(finishCommand(), output), StreamCommandError::none);

    EXPECT_EQ(
        decodeStreamCommand(
            chunkCommand("\"offset\":0,\"data\":\"!!\""), output),
        StreamCommandError::invalidBase64);
    EXPECT_EQ(output.type, StreamCommandType::begin);
    EXPECT_EQ(output.hash, (std::array<uint8_t, 32>{}));
    EXPECT_EQ(output.size, 0u);
    EXPECT_EQ(output.offset, 0u);
    EXPECT_EQ(output.data, (std::array<uint8_t, 256>{}));
    EXPECT_EQ(output.dataSize, 0u);
    EXPECT_EQ(output.signature, (std::array<uint8_t, 64>{}));
}

TEST(StreamCommandTest, FormatsHashAsLowercaseHex) {
    std::array<uint8_t, 32> hash{};
    hash[0] = 0x01;
    hash[1] = 0xAB;
    hash[31] = 0xFF;

    EXPECT_EQ(streamHashText(hash), "01ab" + std::string(58, '0') + "ff");
}

TEST(StreamCommandTest, ClassifiesFinalBytesWithoutOverflow) {
    EXPECT_EQ(
        classifyStreamChunk(
            UINT32_MAX, UINT32_MAX - 255, UINT32_MAX - 255, 255),
        StreamChunkPosition::next);
    EXPECT_EQ(
        classifyStreamChunk(
            UINT32_MAX, UINT32_MAX - 255, UINT32_MAX - 255, 256),
        StreamChunkPosition::invalid);
    EXPECT_EQ(
        classifyStreamChunk(UINT32_MAX, UINT32_MAX, UINT32_MAX - 255, 255),
        StreamChunkPosition::duplicate);
}

TEST(StreamCommandTest, ClassifiesChunkBoundsAndOrder) {
    EXPECT_EQ(classifyStreamChunk(10, 0, 0, 1), StreamChunkPosition::next);
    EXPECT_EQ(classifyStreamChunk(10, 5, 2, 3), StreamChunkPosition::duplicate);
    EXPECT_EQ(classifyStreamChunk(10, 5, 5, 5), StreamChunkPosition::next);
    EXPECT_EQ(classifyStreamChunk(10, 5, 6, 1), StreamChunkPosition::invalid);
    EXPECT_EQ(classifyStreamChunk(10, 5, 2, 4), StreamChunkPosition::invalid);
    EXPECT_EQ(classifyStreamChunk(10, 5, 0, 0), StreamChunkPosition::invalid);
    EXPECT_EQ(classifyStreamChunk(10, 5, 0, 257), StreamChunkPosition::invalid);
    EXPECT_EQ(classifyStreamChunk(10, 11, 5, 1), StreamChunkPosition::invalid);
    EXPECT_EQ(classifyStreamChunk(10, 5, 11, 1), StreamChunkPosition::invalid);
    EXPECT_EQ(classifyStreamChunk(10, 5, 10, 1), StreamChunkPosition::invalid);
}
