#ifndef TEST_FAKE_OTA_FILE_SYSTEM_HPP
#define TEST_FAKE_OTA_FILE_SYSTEM_HPP

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "common/OtaFile.hpp"

class FakeOtaFileSystem final : public OtaFileSystem {
public:
    class PowerCut final {};

    FakeOtaFileSystem() = default;
    ~FakeOtaFileSystem() override = default;

    OtaFilePresence inspect(const char* path) override;
    std::unique_ptr<OtaFile> open(const char* path, OtaFileMode mode) override;
    bool remove(const char* path) override;

    void seed(const char* path, std::string contents);
    std::optional<std::string> contents(const char* path) const;
    size_t bytesRead(const char* path) const;
    size_t operations() const;
    const std::vector<std::string>& operationLog() const;
    void failBefore(size_t operationIndex);
    void tearWriteAfter(size_t bytes);
    void shortWriteAfter(size_t bytes);
    void setInspectError(const char* path, bool enabled = true);
    void setOpenError(const char* path, bool enabled = true);
    void setReadErrorAfter(const char* path, size_t bytes);
    void setCloseFailure(const char* path, bool enabled = true);
    void mutateOnNextReadOpen(
        const char* path, std::string from, std::string to);
    bool readOpenMutationApplied() const;
    void reboot();

private:
    class File;

    void beginOperation(const std::string& operation);
    bool isEnabled(
        const std::map<std::string, bool>& controls,
        const std::string& path) const;

    std::map<std::string, std::string> files;
    std::map<std::string, bool> inspectErrors;
    std::map<std::string, bool> openErrors;
    std::map<std::string, size_t> readErrorOffsets;
    std::map<std::string, size_t> readByteCounts;
    std::map<std::string, bool> closeFailures;
    std::string readOpenMutationPath;
    std::string readOpenMutationFrom;
    std::string readOpenMutationTo;
    bool didApplyReadOpenMutation = false;
    std::optional<size_t> failOperation;
    std::optional<size_t> tearWriteBytes;
    std::optional<size_t> shortWriteBytes;
    std::vector<std::string> operationHistory;
    size_t operationCount = 0;
    size_t generation = 0;

    friend class File;
};

#endif  // TEST_FAKE_OTA_FILE_SYSTEM_HPP
