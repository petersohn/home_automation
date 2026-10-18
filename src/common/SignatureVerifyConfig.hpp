#ifndef COMMON_SIGNATUREVERIFYCONFIG_HPP
#define COMMON_SIGNATUREVERIFYCONFIG_HPP

#include <string>

/** Absolute SPIFFS path to the trusted PEM certificate; existence is checked
 * when the interface is constructed.
 */
struct SignatureVerifyConfig {
    std::string certificate;
};

#endif  // COMMON_SIGNATUREVERIFYCONFIG_HPP
