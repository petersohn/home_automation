#ifndef TOOLS_STRICTBASE64_HPP
#define TOOLS_STRICTBASE64_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tools {

// Decodes a canonical RFC 4648 base64 string using the standard alphabet.
//
// Strictness requirements (the permissive bundled libb64 decoder is only
// used after this validation succeeds):
// - Standard alphabet A-Z a-z 0-9 + / only. URL-safe characters (- and _),
//   whitespace anywhere in the input (leading, trailing or embedded), and
//   any other invalid character are rejected.
// - Correct padding: the length must be a nonzero multiple of 4 and '=' only
//   appears as one or two final padding characters. Missing padding, extra
//   padding, or padding anywhere other than the very end are rejected.
// - Canonical output: unused bits in the final partial group must be zero.
// - Empty input is valid and decodes to zero bytes.
//
// maxOutput bounds the decoded size; an input that would decode to more than
// maxOutput bytes fails before any large buffer is produced.
//
// On success returns true and output holds exactly the decoded bytes
// (replacing any previous contents). On any failure returns false and output
// is cleared, so the output is valid only after a successful call.
bool decodeCanonicalBase64(
    const std::string& input, size_t maxOutput, std::vector<uint8_t>& output);

}  // namespace tools

#endif  // TOOLS_STRICTBASE64_HPP
