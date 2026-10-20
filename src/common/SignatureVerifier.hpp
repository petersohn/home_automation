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
    hashMismatch,
    invalidState,
    internalError
};

struct SignatureResult {
    SignatureStatus status;
    SignatureError error;
};

/** Incrementally hashes data and verifies its digest against a raw signature.
 * begin resets prior state; finish consumes the stream; abort is idempotent.
 * Initialization errors must remain fail-closed across lifecycle calls.
 */
class SignatureVerifier {
public:
    virtual ~SignatureVerifier() = default;

    virtual SignatureError begin() = 0;
    virtual SignatureError append(SignatureBytes data) = 0;
    virtual SignatureResult finish(
        SignatureBytes expectedHash, SignatureBytes signature) = 0;
    virtual void abort() = 0;
};

#endif  // COMMON_SIGNATUREVERIFIER_HPP
