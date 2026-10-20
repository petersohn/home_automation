#ifndef BEARSSLSIGNATUREVERIFIER_HPP
#define BEARSSLSIGNATUREVERIFIER_HPP

#include <ESP8266WiFi.h>
#include <bearssl/bearssl.h>

#include <cstddef>
#include <cstdint>

#include "common/SignatureVerifier.hpp"

/**
 * Verifies raw 64-byte P-256 ECDSA signatures using BearSSL.
 *
 * Accepts one PEM certificate with surrounding ASCII whitespace and requires
 * a P-256 key; DER, extra PEM objects, and junk are rejected. Provisioning is
 * the trust boundary; chains, hostname, time, and self-signatures are not
 * validated. The verifier owns parsed key material, so input bytes may be
 * released after construction. Initialization errors are retained and all
 * lifecycle calls fail closed.
 *
 * BearSSL's wrapper expects a 65-byte public-point-sized signature; this
 * backend verifies raw r||s directly. Verification uses the core secondary
 * stack because BearSSL's i31 implementation exceeds the main stack.
 */
class BearSslSignatureVerifier : public SignatureVerifier {
public:
    explicit BearSslSignatureVerifier(SignatureBytes certificateBytes);

    explicit BearSslSignatureVerifier(SignatureError error);

    ~BearSslSignatureVerifier() override;

    BearSslSignatureVerifier(const BearSslSignatureVerifier&) = delete;
    BearSslSignatureVerifier& operator=(const BearSslSignatureVerifier&) =
        delete;

    SignatureError begin() override;
    SignatureError append(SignatureBytes data) override;
    SignatureResult finish(
        SignatureBytes expectedHash, SignatureBytes signature) override;
    void abort() override;

private:
    BearSSL::X509List certificate;

    // Points into certificate and remains valid for its lifetime.
    const br_x509_trust_anchor* trustAnchor = nullptr;

    SignatureError initError = SignatureError::none;
    br_sha256_context hashContext{};
    bool active = false;

    // Error-state instances do not acquire the thunk stack reference.
    bool thunkStackReferenced = false;
};

#endif  // BEARSSLSIGNATUREVERIFIER_HPP
