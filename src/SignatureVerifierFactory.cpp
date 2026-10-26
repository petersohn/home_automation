#include "SignatureVerifierFactory.hpp"

#include <FS.h>

#include <cstddef>
#include <memory>
#include <new>

#include "BearSslSignatureVerifier.hpp"

std::unique_ptr<SignatureVerifier> loadSignatureVerifier(
    std::ostream& debug, const char* certificatePath) {
    static constexpr size_t maxCertificateSize = 4096;
    const char* const path = certificatePath == nullptr ? "" : certificatePath;

    File certificateFile = SPIFFS.open(path, "r");
    if (!certificateFile) {
        debug << "Could not open certificate: " << path << std::endl;
        return std::make_unique<BearSslSignatureVerifier>(
            SignatureError::certificateUnavailable);
    }

    std::unique_ptr<SignatureVerifier> verifier;
    const size_t certificateSize = certificateFile.size();
    if (certificateSize == 0) {
        debug << "Empty certificate file: " << path << std::endl;
        verifier = std::make_unique<BearSslSignatureVerifier>(
            SignatureError::invalidCertificate);
    } else if (certificateSize > maxCertificateSize) {
        debug << "Certificate too large: " << path << std::endl;
        verifier = std::make_unique<BearSslSignatureVerifier>(
            SignatureError::invalidCertificate);
    } else {
        std::unique_ptr<uint8_t[]> certificateBytes(
            new (std::nothrow) uint8_t[certificateSize]);
        if (!certificateBytes) {
            debug << "Out of memory for certificate: " << path << std::endl;
            verifier = std::make_unique<BearSslSignatureVerifier>(
                SignatureError::internalError);
        } else {
            const int readLength =
                certificateFile.read(certificateBytes.get(), certificateSize);
            if (readLength < 0 ||
                static_cast<size_t>(readLength) != certificateSize) {
                debug << "Could not read certificate: " << path << std::endl;
                verifier = std::make_unique<BearSslSignatureVerifier>(
                    SignatureError::certificateUnavailable);
            } else {
                verifier = std::make_unique<BearSslSignatureVerifier>(
                    SignatureBytes{certificateBytes.get(), certificateSize});
            }
        }
    }

    certificateFile.close();
    return verifier;
}
