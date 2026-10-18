#include "BearSslSignatureVerifier.hpp"

#include <StackThunk.h>

namespace {

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

// BearSSL's i31 verifier can exceed the main stack (~6 KB); use the secondary
// stack thunk. Input pointers remain valid across the stack switch.
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
    SignatureBytes certificateBytes) {
    // X509List accepts DER and multiple PEM certificates; enforce one PEM block.
    if (!isSinglePemCertificate(certificateBytes.data, certificateBytes.size)) {
        this->initError = SignatureError::invalidCertificate;
        return;
    }

    size_t certificateStartOffset = 0;
    while (isPemWhitespace(certificateBytes.data[certificateStartOffset])) {
        ++certificateStartOffset;
    }
    if (!this->certificate.append(
            certificateBytes.data + certificateStartOffset,
            certificateBytes.size - certificateStartOffset) ||
        this->certificate.getCount() != 1) {
        this->initError = SignatureError::invalidCertificate;
        return;
    }

    this->trustAnchor = this->certificate.getTrustAnchors();
    if (!isP256TrustAnchor(this->trustAnchor)) {
        this->initError = SignatureError::unsupportedKey;
        return;
    }

    // Only initialized instances invoke the thunk and need its stack reference.
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
    if (this->initError != SignatureError::none) {
        return SignatureResult{SignatureStatus::error, this->initError};
    }

    if ((data.size != 0 && data.data == nullptr) ||
        (signature.size != 0 && signature.data == nullptr)) {
        return SignatureResult{
            SignatureStatus::error, SignatureError::internalError};
    }

    if (signature.size != p256SignatureLength) {
        return SignatureResult{
            SignatureStatus::error, SignatureError::invalidSignatureLength};
    }

    br_sha256_context hashContext;
    br_sha256_init(&hashContext);
    if (data.size != 0) {
        br_sha256_update(&hashContext, data.data, data.size);
    }
    uint8_t digest[sha256Length];
    br_sha256_out(&hashContext, digest);

    // Invalid r/s values are verification failures, not backend errors.
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
