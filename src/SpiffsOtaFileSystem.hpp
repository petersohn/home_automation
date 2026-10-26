#ifndef SPIFFS_OTA_FILE_SYSTEM_HPP
#define SPIFFS_OTA_FILE_SYSTEM_HPP

#include "common/OtaFile.hpp"

class SpiffsOtaFileSystem final : public OtaFileSystem {
public:
    bool mount();
    bool mounted() const;

    OtaFilePresence inspect(const char* path) override;
    std::unique_ptr<OtaFile> open(const char* path, OtaFileMode mode) override;
    bool remove(const char* path) override;

private:
    bool mountAttempted = false;
    bool isMounted = false;
};

#endif  // SPIFFS_OTA_FILE_SYSTEM_HPP
