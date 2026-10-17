#ifndef COMMON_SIGNEDENVELOPE_HPP
#define COMMON_SIGNEDENVELOPE_HPP

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Strict decoder for the signed-envelope wire format: one MQTT payload
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
// 2. JSON structure: the command must be a single JSON document containing
//    only an object with exactly the fields data and signature, both strings.
//    JSON whitespace between tokens and key order are irrelevant; escaped key
//    names and escaped characters are unescaped first, so "\u0064ata" is
//    recognized as the data field and "\u0041..." value content counts as its
//    unescaped characters. Malformed syntax, invalid escapes, raw control
//    characters, and trailing non-whitespace data are rejected (invalidJson).
//    Parseable JSON with wrong structure - non-object root, unknown, missing,
//    or duplicate fields (duplicates are detected before map collapse,
//    including escape-aliased ones), or non-string/nested field values - is
//    rejected (invalidFields).
// 3. Limits before decoding buffers: data content that would decode to more
//    than 256 bytes is rejected (dataTooLarge) based on its length.
// 4. Base64: both fields must be canonical RFC 4648 (strict; see
//    tools::decodeCanonicalBase64) after unescaping (invalidBase64).
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
