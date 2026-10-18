#ifndef BEARSSLSIGNATUREVERIFIER_HPP
#define BEARSSLSIGNATUREVERIFIER_HPP

#include <ESP8266WiFi.h>
#include <bearssl/bearssl.h>

#include <cstddef>
#include <cstdint>

#include "common/SignatureVerifier.hpp"

// Device-specific SignatureVerifier backend built on the BearSSL library
// bundled with the ESP8266 core (core version 3.1.2).
//
// Initialization takes exactly one PEM X.509 certificate with only
// surrounding whitespace, extracts the pinned public key, and requires it to
// be an ECDSA P-256 key. DER, concatenated or other PEM objects, and junk are
// rejected. No certificate chain, hostname, time validity, or self-signature
// validation is performed: trust comes entirely from provisioning. Every
// failure retains a structured SignatureError, and every attempted
// verification on a failed verifier fails closed with that error.
//
// Lifetime: the verifier owns all key material (the parsed certificate and a
// heap copy of the public point inside BearSSL::X509List), so the
// certificate bytes used for initialization may be released immediately
// after construction. The retained public key pointers remain valid for the
// verifier's lifetime.
//
// The bundled BearSSL::SigningVerifier wrapper is intentionally not used: its
// length() returns the EC public-key point length (65 for P-256) and its
// verify() rejects anything else, while this component's wire format is a
// 64-byte raw r||s signature. The raw ECDSA verifier is used directly, with
// the explicit 64-byte check here. ECDSA verification is stack-heavy, so the
// call runs on the core-managed secondary stack (see the thunk in the
// implementation, mirroring BearSSLHelpers.cpp).
class BearSslSignatureVerifier : public SignatureVerifier {
public:
    // Parses one strict PEM certificate from the byte view. Initialization
    // failures (malformed, empty, multiple certificates, unsupported key) are
    // retained in the state, never thrown.
    explicit BearSslSignatureVerifier(SignatureBytes certificateBytes);

    // Error-state verifier that performs no parsing; every attempted
    // verification fails closed with the retained error. Used when the
    // certificate cannot even be loaded from the filesystem.
    explicit BearSslSignatureVerifier(SignatureError error);

    ~BearSslSignatureVerifier() override;

    BearSslSignatureVerifier(const BearSslSignatureVerifier&) = delete;
    BearSslSignatureVerifier& operator=(const BearSslSignatureVerifier&) =
        delete;

    SignatureResult verify(
        SignatureBytes data, SignatureBytes signature) override;

private:
    // Owns the parsed certificate and extracted public key material; all
    // key pointers are heap copies, not views into the input bytes.
    BearSSL::X509List certificate;

    // Points into certificate's trust anchor; valid only when initError is
    // none, because certificate is never mutated after construction.
    const br_x509_trust_anchor* trustAnchor = nullptr;

    SignatureError initError = SignatureError::none;

    // Whether this instance holds a reference on the secondary-stack thunk
    // stack; false for error-state instances that can never verify.
    bool thunkStackReferenced = false;
};

#endif  // BEARSSLSIGNATUREVERIFIER_HPP
