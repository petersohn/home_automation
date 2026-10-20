#ifndef COMMON_STREAMCOMMAND_HPP
#define COMMON_STREAMCOMMAND_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

enum class StreamCommandType { begin, chunk, finish };

enum class StreamCommandError {
    none,
    invalidJson,
    invalidFields,
    invalidHash,
    invalidBase64,
    commandTooLarge,
    dataTooLarge,
    invalidSignatureLength
};

struct StreamCommand {
    StreamCommandType type = StreamCommandType::begin;
    std::array<uint8_t, 32> hash{};
    uint32_t size = 0;
    uint32_t offset = 0;
    std::array<uint8_t, 256> data{};
    size_t dataSize = 0;
    std::array<uint8_t, 64> signature{};
};

StreamCommandError decodeStreamCommand(
    const std::string& text, StreamCommand& output);
std::string streamHashText(const std::array<uint8_t, 32>& hash);

enum class StreamChunkPosition { next, duplicate, invalid };

StreamChunkPosition classifyStreamChunk(
    uint32_t total, uint32_t received, uint32_t offset, size_t length);

#endif  // COMMON_STREAMCOMMAND_HPP
