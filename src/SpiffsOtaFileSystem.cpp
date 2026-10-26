#include "SpiffsOtaFileSystem.hpp"

#include <FS.h>
#include <flash_hal.h>
#include <spiffs_api.h>

#include <memory>
#include <new>
#include <utility>

#include "common/OtaFileLookup.hpp"

namespace {

#ifndef SPIFFS_MAX_OPEN_FILES
#define SPIFFS_MAX_OPEN_FILES 5
#endif

class ErrorAwareSpiffsImpl final : public spiffs_impl::SPIFFSImpl {
public:
    ErrorAwareSpiffsImpl()
        : SPIFFSImpl(
              FS_PHYS_ADDR, FS_PHYS_SIZE, FS_PHYS_PAGE, FS_PHYS_BLOCK,
              SPIFFS_MAX_OPEN_FILES) {}

    int statCode(const char* path) {
        if (!spiffs_impl::isSpiffsFilenameValid(path) ||
            SPIFFS_mounted(this->getFs()) == 0) {
            return SPIFFS_ERR_INTERNAL;
        }
        spiffs_stat stat{};
        return SPIFFS_stat(this->getFs(), path, &stat);
    }
};

ErrorAwareSpiffsImpl& errorAwareSpiffs() {
    static std::shared_ptr<ErrorAwareSpiffsImpl> implementation =
        std::make_shared<ErrorAwareSpiffsImpl>();
    static bool installed = false;
    if (!installed) {
        SPIFFS = fs::FS(implementation);
        installed = true;
    }
    return *implementation;
}

class SpiffsOtaFile final : public OtaFile {
public:
    SpiffsOtaFile(File file, OtaFileMode mode)
        : file(std::move(file)), mode(mode) {
        if (mode == OtaFileMode::read) {
            this->expectedSize = this->file.size();
        }
    }

    int read() override {
        if (this->closed || this->mode != OtaFileMode::read || this->ioFailed) {
            this->ioFailed = true;
            return -2;
        }
        if (this->bytesRead == this->expectedSize) {
            return -1;
        }

        const int value = this->file.read();
        if (value < 0 || value > 255) {
            this->ioFailed = true;
            return -2;
        }
        ++this->bytesRead;
        return value;
    }

    size_t write(SignatureBytes bytes) override {
        if (this->closed || this->mode != OtaFileMode::truncate ||
            this->ioFailed || (bytes.size != 0 && bytes.data == nullptr)) {
            this->ioFailed = true;
            return 0;
        }

        const size_t written = this->file.write(bytes.data, bytes.size);
        if (written != bytes.size) {
            this->ioFailed = true;
        }
        return written;
    }

    bool close() override {
        if (this->closed) {
            return !this->ioFailed;
        }
        if (this->mode == OtaFileMode::truncate) {
            this->file.flush();
        }
        this->file.close();
        this->closed = true;
        return !this->ioFailed;
    }

    bool failed() const override { return this->ioFailed; }

private:
    File file;
    OtaFileMode mode;
    size_t expectedSize = 0;
    size_t bytesRead = 0;
    bool ioFailed = false;
    bool closed = false;
};

}  // namespace

bool SpiffsOtaFileSystem::mount() {
    if (this->isMounted) {
        return true;
    }
    if (this->mountAttempted) {
        return false;
    }
    this->mountAttempted = true;

    (void)errorAwareSpiffs();
    const SPIFFSConfig config(false);
    if (!SPIFFS.setConfig(config)) {
        return false;
    }
    this->isMounted = SPIFFS.begin();
    return this->isMounted;
}

bool SpiffsOtaFileSystem::mounted() const {
    return this->isMounted;
}

OtaFilePresence SpiffsOtaFileSystem::inspect(const char* path) {
    if (!this->isMounted || path == nullptr || path[0] == '\0') {
        return OtaFilePresence::error;
    }

    const int result = errorAwareSpiffs().statCode(path);
    return classifyOtaStatResult(result, SPIFFS_OK, SPIFFS_ERR_NOT_FOUND);
}

std::unique_ptr<OtaFile> SpiffsOtaFileSystem::open(
    const char* path, OtaFileMode mode) {
    if (!this->isMounted || path == nullptr || path[0] == '\0') {
        return nullptr;
    }

    File file = SPIFFS.open(path, mode == OtaFileMode::read ? "r" : "w");
    if (!file) {
        return nullptr;
    }

    SpiffsOtaFile* const result =
        new (std::nothrow) SpiffsOtaFile(std::move(file), mode);
    if (result == nullptr) {
        file.close();
        return nullptr;
    }
    return std::unique_ptr<OtaFile>(result);
}

bool SpiffsOtaFileSystem::remove(const char* path) {
    if (!this->isMounted || path == nullptr || path[0] == '\0') {
        return false;
    }
    return removeOtaFileIfPresent(
        this->inspect(path), [path]() { return SPIFFS.remove(path); });
}
