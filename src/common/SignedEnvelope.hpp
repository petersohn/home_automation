#ifndef COMMON_SIGNEDENVELOPE_HPP
#define COMMON_SIGNEDENVELOPE_HPP

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Decoder for the signed-envelope wire format: one MQTT payload
// carrying a UTF-8 JSON object with exactly two string fields:
//
//   {"data":"<base64>","signature":"<base64>"}
//
// data holds the prototype payload (at most 256 decoded bytes, empty data is
// allowed) and signature holds the raw ECDSA signature (exactly 64 decoded
// bytes, big-endian r followed by s). Signature bytes are signed as decoded;
// no JSON canonicalization or sender-selected algorithm exists.

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

// Decodes a signed envelope. Checks, in order:
//
// 1. Oversize command: a command longer than 512 bytes is rejected
//    (commandTooLarge) without parsing it, before copying into any storage.
// 2. JSON is parsed directly by ArduinoJson. Its native syntax and escape
//    behavior applies; no Unicode escape normalization is performed. The root
//    must be an object with exactly two fields, data and signature, both
//    strings. Whitespace and key order are irrelevant. Duplicate literal keys
//    use ArduinoJson's last-value-wins behavior. Embedded NUL and trailing
//    non-whitespace data are rejected (invalidJson). Parseable documents with
//    the wrong envelope shape, unknown/missing fields, or non-string values
//    are rejected (invalidFields).
// 3. Limits before decoding buffers: data content that would decode to more
//    than 256 bytes is rejected (dataTooLarge) based on its length.
// 4. Base64: both fields must be canonical RFC 4648 (strict; see
//    tools::decodeCanonicalBase64) after ArduinoJson string decoding
//    (invalidBase64).
// 5. Signature length: the decoded signature must be exactly 64 bytes
//    (invalidSignatureLength).
//
// Cryptographic verification is not part of decoding.
//
// On success returns EnvelopeError::none and output holds the decoded data
// bytes and signature. On failure returns the error above and output is
// reset (data emptied, signature zeroed), so the output is valid only after
// a successful call.
EnvelopeError decodeSignedEnvelope(
    const std::string& command, SignedEnvelope& output);

#endif  // COMMON_SIGNEDENVELOPE_HPP
