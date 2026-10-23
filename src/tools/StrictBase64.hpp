#ifndef TOOLS_STRICTBASE64_HPP
#define TOOLS_STRICTBASE64_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tools {

/**
 * Decodes canonical RFC 4648 base64; empty input is valid. Rejects nonstandard
 * characters, whitespace, invalid padding, and nonzero unused bits. maxOutput
 * bounds decoded size. Success replaces output; failure clears it.
 */
bool decodeCanonicalBase64(
    const std::string& input, size_t maxOutput, std::vector<uint8_t>& output);

size_t decodedUpperBound(const std::string& input);

}  // namespace tools

#endif  // TOOLS_STRICTBASE64_HPP
