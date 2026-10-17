#ifndef COMMON_SIGNATUREVERIFIER_HPP
#define COMMON_SIGNATUREVERIFIER_HPP

#include <cstddef>
#include <cstdint>

// Byte view, structured result, and abstract verifier contract for signed
// data verification. Portable only: no MQTT, JSON/base64, filesystem paths,
// action dispatch, device types, or presentation strings appear here.

struct SignatureBytes {
    const uint8_t* data;
    size_t size;
};

enum class SignatureStatus { valid, invalid, error };

enum class SignatureError {
    none,
    invalidSignature,
    invalidCertificate,
    unsupportedKey,
    certificateUnavailable,
    invalidSignatureLength,
    internalError
};

struct SignatureResult {
    SignatureStatus status;
    SignatureError error;
};

// Abstract verifier. Hashes the data referenced by data (including empty
// data) and verifies the raw signature bytes against it, returning a
// structured result. A non-null pointer is required for a nonempty view; an
// empty view is permitted. Error enum codes are mapped to stable result
// strings only in the interface layer, never in implementations. Once a
// verifier's initialization failed, every attempted verification must fail
// closed with the retained error.
class SignatureVerifier {
public:
    virtual ~SignatureVerifier() = default;

    virtual SignatureResult verify(
        SignatureBytes data, SignatureBytes signature) = 0;
};

#endif  // COMMON_SIGNATUREVERIFIER_HPP
