#ifndef COMMON_OTA_FILE_HPP
#define COMMON_OTA_FILE_HPP

#include <cstddef>
#include <memory>

#include "OtaTypes.hpp"
#include "SignatureVerifier.hpp"

class OtaFile {
public:
    virtual ~OtaFile() = default;

    // Returns a byte, -1 at EOF, or -2 on I/O failure.
    virtual int read() = 0;
    virtual size_t write(SignatureBytes bytes) = 0;
    virtual bool close() = 0;
    virtual bool failed() const = 0;
};

enum class OtaFilePresence { missing, present, error };
enum class OtaFileMode { read, truncate };

class OtaFileSystem {
public:
    virtual ~OtaFileSystem() = default;

    virtual OtaFilePresence inspect(const char* path) = 0;
    virtual std::unique_ptr<OtaFile> open(
        const char* path, OtaFileMode mode) = 0;
    virtual bool remove(const char* path) = 0;
};

#endif  // COMMON_OTA_FILE_HPP
