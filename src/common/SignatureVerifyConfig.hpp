#ifndef COMMON_SIGNATUREVERIFYCONFIG_HPP
#define COMMON_SIGNATUREVERIFYCONFIG_HPP

#include <string>

// Configuration for the signatureVerify interface.
// certificate holds an absolute SPIFFS path to a single PEM X.509
// certificate whose public key is pinned as the trusted signing key. A
// relative path must be rejected during parsing; whether the file exists is
// only checked at config factory time.
struct SignatureVerifyConfig {
    std::string certificate;
};

#endif  // COMMON_SIGNATUREVERIFYCONFIG_HPP
