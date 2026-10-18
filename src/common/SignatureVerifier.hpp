#ifndef COMMON_SIGNATUREVERIFIER_HPP
#define COMMON_SIGNATUREVERIFIER_HPP

#include <cstddef>
#include <cstdint>

/** Non-owning byte view; data must stay valid during use and be non-null when
 * size is nonzero.
 */
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

/** Hashes data, including empty data, and verifies the raw signature bytes.
 * Initialization failures must remain fail-closed with their retained error.
 */
class SignatureVerifier {
public:
    virtual ~SignatureVerifier() = default;

    virtual SignatureResult verify(
        SignatureBytes data, SignatureBytes signature) = 0;
};

#endif  // COMMON_SIGNATUREVERIFIER_HPP
