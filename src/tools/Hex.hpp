#ifndef TOOLS_HEX_HPP
#define TOOLS_HEX_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace tools {

template <std::size_t N>
bool decodeLowercaseHex(
    const std::string& text, std::array<uint8_t, N>& output) {
    if (text.size() != N * 2) {
        return false;
    }

    std::array<uint8_t, N> decoded{};
    for (std::size_t i = 0; i < N; ++i) {
        const char high = text[2 * i];
        const char low = text[2 * i + 1];
        if (!((high >= '0' && high <= '9') || (high >= 'a' && high <= 'f')) ||
            !((low >= '0' && low <= '9') || (low >= 'a' && low <= 'f'))) {
            return false;
        }
        const uint8_t highValue = static_cast<uint8_t>(
            high >= '0' && high <= '9' ? high - '0' : high - 'a' + 10);
        const uint8_t lowValue = static_cast<uint8_t>(
            low >= '0' && low <= '9' ? low - '0' : low - 'a' + 10);
        decoded[i] = static_cast<uint8_t>((highValue << 4) | lowValue);
    }
    output = decoded;
    return true;
}

}  // namespace tools

#endif  // TOOLS_HEX_HPP
