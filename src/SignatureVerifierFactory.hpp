#ifndef SIGNATURE_VERIFIER_FACTORY_HPP
#define SIGNATURE_VERIFIER_FACTORY_HPP

#include <memory>
#include <ostream>

#include "common/SignatureVerifier.hpp"

std::unique_ptr<SignatureVerifier> loadSignatureVerifier(
    std::ostream& debug, const char* certificatePath);

#endif  // SIGNATURE_VERIFIER_FACTORY_HPP
