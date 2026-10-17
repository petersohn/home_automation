#include "BearSslSignatureVerifier.hpp"

#include <StackThunk.h>

namespace {

// Raw ECDSA P-256 signature: big-endian r followed by big-endian s, 32 bytes
// each. The uncompressed P-256 public point is 1 + 32 + 32 bytes.
constexpr size_t p256SignatureLength = 64;
constexpr size_t p256PointLength = 65;
constexpr size_t sha256Length = 32;
constexpr char pemCertificateStart[] = "-----BEGIN CERTIFICATE-----";
constexpr char pemCertificateEnd[] = "-----END CERTIFICATE-----";

bool isPemWhitespace(uint8_t value) {
    return value == ' ' || value == '\t' || value == '\n' || value == '\r' ||
           value == '\v' || value == '\f';
}

bool isBase64Character(uint8_t value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9') || value == '+' || value == '/' ||
           value == '=';
}

bool matchesAt(
    const uint8_t* bytes, size_t length, size_t offset, const char* text,
    size_t textLength) {
    if (offset > length || textLength > length - offset) {
        return false;
    }
    for (size_t i = 0; i < textLength; ++i) {
        if (bytes[offset + i] != static_cast<uint8_t>(text[i])) {
            return false;
        }
    }
    return true;
}

bool isSinglePemCertificate(const uint8_t* bytes, size_t length) {
    if (bytes == nullptr || length == 0) {
        return false;
    }

    constexpr size_t startLength = sizeof(pemCertificateStart) - 1;
    constexpr size_t endLength = sizeof(pemCertificateEnd) - 1;

    size_t startOffset = 0;
    while (startOffset < length && isPemWhitespace(bytes[startOffset])) {
        ++startOffset;
    }
    if (!matchesAt(
            bytes, length, startOffset, pemCertificateStart, startLength)) {
        return false;
    }

    const size_t bodyOffset = startOffset + startLength;
    if (length - bodyOffset < endLength) {
        return false;
    }

    size_t endOffset = bodyOffset;
    const size_t lastPossibleEnd = length - endLength;
    while (endOffset <= lastPossibleEnd &&
           !matchesAt(bytes, length, endOffset, pemCertificateEnd, endLength)) {
        ++endOffset;
    }
    if (endOffset > lastPossibleEnd) {
        return false;
    }

    bool hasBase64Character = false;
    for (size_t i = bodyOffset; i < endOffset; ++i) {
        if (isPemWhitespace(bytes[i])) {
            continue;
        }
        if (!isBase64Character(bytes[i])) {
            return false;
        }
        hasBase64Character = true;
    }
    if (!hasBase64Character) {
        return false;
    }

    for (size_t i = endOffset + endLength; i < length; ++i) {
        if (!isPemWhitespace(bytes[i])) {
            return false;
        }
    }
    return true;
}

bool isP256TrustAnchor(const br_x509_trust_anchor* anchor) {
    if (!anchor) {
        return false;
    }
    if (anchor->pkey.key_type != BR_KEYTYPE_EC) {
        return false;
    }
    if (anchor->pkey.key.ec.curve != BR_EC_secp256r1) {
        return false;
    }
    return anchor->pkey.key.ec.qlen == p256PointLength;
}

}  // unnamed namespace

// ECDSA verification needs more stack than the Arduino main stack provides
// (up to ~6 KB for the i31 implementation), so the call is executed on the
// secondary stack through the StackThunk mechanism, using the same pattern as
// the bundled BearSSLHelpers.cpp (extern "C" worker + make_stack_thunk +
// thunk_ declaration). The hash, signature, and key live outside the
// secondary stack and are accessed through their pointers, which stay valid
// while the stack pointer switches.
extern "C" bool BearSslSignatureVerifier_verifyRaw(
    const br_ec_public_key* key, const void* hash, size_t hashLength,
    const void* signature, size_t signatureLength) {
    br_ecdsa_vrfy vrfy = br_ecdsa_vrfy_raw_get_default();
    return vrfy(
        br_ec_get_default(), hash, hashLength, key, signature, signatureLength);
}

#if !CORE_MOCK
make_stack_thunk(BearSslSignatureVerifier_verifyRaw);
extern "C" bool thunk_BearSslSignatureVerifier_verifyRaw(
    const br_ec_public_key* key, const void* hash, size_t hashLength,
    const void* signature, size_t signatureLength);
#endif

BearSslSignatureVerifier::BearSslSignatureVerifier(
    const uint8_t* certificateBytes, size_t certificateLength) {
    // BearSSL's X509List parser also accepts DER and ignores content between
    // PEM objects. Validate the complete input envelope before parsing so only
    // one CERTIFICATE block with surrounding whitespace reaches that parser.
    if (!isSinglePemCertificate(certificateBytes, certificateLength)) {
        this->initError = SignatureError::invalidCertificate;
        return;
    }

    size_t certificateStartOffset = 0;
    while (isPemWhitespace(certificateBytes[certificateStartOffset])) {
        ++certificateStartOffset;
    }
    if (!this->certificate.append(
            certificateBytes + certificateStartOffset,
            certificateLength - certificateStartOffset) ||
        this->certificate.getCount() != 1) {
        this->initError = SignatureError::invalidCertificate;
        return;
    }

    this->trustAnchor = this->certificate.getTrustAnchors();
    if (!isP256TrustAnchor(this->trustAnchor)) {
        this->initError = SignatureError::unsupportedKey;
        return;
    }

    // Hold a reference on the secondary stack for this verifier's lifetime;
    // only successfully initialized verifiers ever invoke the thunk.
    stack_thunk_add_ref();
    this->thunkStackReferenced = true;
}

BearSslSignatureVerifier::BearSslSignatureVerifier(SignatureError error)
    : initError(error) {}

BearSslSignatureVerifier::~BearSslSignatureVerifier() {
    if (this->thunkStackReferenced) {
        stack_thunk_del_ref();
    }
}

SignatureResult BearSslSignatureVerifier::verify(
    SignatureBytes data, SignatureBytes signature) {
    // A verifier whose initialization failed fails closed with the retained
    // error; it never falls back to success.
    if (this->initError != SignatureError::none) {
        return SignatureResult{SignatureStatus::error, this->initError};
    }

    // A nonempty view requires a non-null pointer; a null pointer with a
    // nonzero size is an input error that fails closed.
    if ((data.size != 0 && data.data == nullptr) ||
        (signature.size != 0 && signature.data == nullptr)) {
        return SignatureResult{
            SignatureStatus::error, SignatureError::internalError};
    }

    // Exactly a 64-byte raw signature is required. The bundled
    // SigningVerifier wrapper would compare against the public-key point
    // length instead, so the check is done explicitly here.
    if (signature.size != p256SignatureLength) {
        return SignatureResult{
            SignatureStatus::error, SignatureError::invalidSignatureLength};
    }

    // SHA-256 over the data (empty data is a valid, well-defined input).
    br_sha256_context hashContext;
    br_sha256_init(&hashContext);
    if (data.size != 0) {
        br_sha256_update(&hashContext, data.data, data.size);
    }
    uint8_t digest[sha256Length];
    br_sha256_out(&hashContext, digest);

    // Raw ECDSA verify on the secondary stack. BearSSL rejects
    // zero or out-of-range r/s values, so those count as invalid
    // signatures, not errors.
#if !CORE_MOCK
    const bool valid = thunk_BearSslSignatureVerifier_verifyRaw(
        &this->trustAnchor->pkey.key.ec, digest, sizeof(digest), signature.data,
        signature.size);
#else
    const bool valid = BearSslSignatureVerifier_verifyRaw(
        &this->trustAnchor->pkey.key.ec, digest, sizeof(digest), signature.data,
        signature.size);
#endif

    if (valid) {
        return SignatureResult{SignatureStatus::valid, SignatureError::none};
    }
    return SignatureResult{
        SignatureStatus::invalid, SignatureError::invalidSignature};
}
