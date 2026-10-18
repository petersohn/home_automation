#ifndef COMMON_SIGNEDENVELOPE_HPP
#define COMMON_SIGNEDENVELOPE_HPP

#include <array>
#include <cstdint>
#include <string>
#include <vector>

enum class EnvelopeError {
    none,
    invalidJson,
    invalidFields,
    invalidBase64,
    commandTooLarge,
    dataTooLarge,
    invalidSignatureLength
};

struct SignedEnvelope {
    std::vector<uint8_t> data;
    std::array<uint8_t, 64> signature;
};

/**
 * Decodes a JSON object with exactly two string fields: data and signature.
 * Commands over 512 bytes are rejected before parsing. Data decodes to at most
 * 256 bytes (empty is allowed); signature is 64-byte big-endian raw ECDSA r||s.
 * Both fields require canonical RFC 4648 base64. Whitespace and key order are
 * allowed; the envelope selects no algorithm and JSON is not canonicalized.
 *
 * ArduinoJson 5 escape behavior applies without Unicode normalization;
 * duplicate literal keys use the last value. Embedded NUL and trailing
 * non-whitespace are rejected. Only decoded data is signed. Success replaces
 * output; failure clears data and zeroes signature. Malformed JSON, invalid
 * fields, size violations, invalid base64, and signature-length errors have
 * distinct EnvelopeError values.
 */
EnvelopeError decodeSignedEnvelope(
    const std::string& command, SignedEnvelope& output);

#endif  // COMMON_SIGNEDENVELOPE_HPP
