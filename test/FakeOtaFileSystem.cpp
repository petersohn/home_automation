#include "FakeOtaFileSystem.hpp"

#include <algorithm>
#include <utility>

class FakeOtaFileSystem::File final : public OtaFile {
public:
    File(
        FakeOtaFileSystem& fileSystem, std::string filePath,
        OtaFileMode fileMode, size_t fileGeneration)
        : fileSystem(fileSystem)
        , filePath(std::move(filePath))
        , fileMode(fileMode)
        , fileGeneration(fileGeneration) {}

    int read() override {
        this->fileSystem.beginOperation("read:" + this->filePath);
        if (!this->isUsable(OtaFileMode::read)) {
            this->hasFailed = true;
            return -2;
        }

        const auto errorOffset =
            this->fileSystem.readErrorOffsets.find(this->filePath);
        if (errorOffset != this->fileSystem.readErrorOffsets.end() &&
            this->position >= errorOffset->second) {
            this->hasFailed = true;
            return -2;
        }

        const auto file = this->fileSystem.files.find(this->filePath);
        if (file == this->fileSystem.files.end()) {
            this->hasFailed = true;
            return -2;
        }
        if (this->position >= file->second.size()) {
            return -1;
        }
        ++this->fileSystem.readByteCounts[this->filePath];
        return static_cast<unsigned char>(file->second[this->position++]);
    }

    size_t write(SignatureBytes bytes) override {
        this->fileSystem.beginOperation("write:" + this->filePath);
        if (!this->isUsable(OtaFileMode::truncate)) {
            this->hasFailed = true;
            return 0;
        }

        const auto file = this->fileSystem.files.find(this->filePath);
        if (file == this->fileSystem.files.end()) {
            this->hasFailed = true;
            return 0;
        }

        if (this->fileSystem.tearWriteBytes.has_value()) {
            const size_t count =
                std::min(bytes.size, *this->fileSystem.tearWriteBytes);
            this->append(file->second, bytes, count);
            this->fileSystem.tearWriteBytes.reset();
            throw PowerCut{};
        }

        if (this->fileSystem.shortWriteBytes.has_value()) {
            const size_t count =
                std::min(bytes.size, *this->fileSystem.shortWriteBytes);
            this->append(file->second, bytes, count);
            this->fileSystem.shortWriteBytes.reset();
            if (count != bytes.size) {
                this->hasFailed = true;
            }
            return count;
        }

        this->append(file->second, bytes, bytes.size);
        return bytes.size;
    }

    bool close() override {
        this->fileSystem.beginOperation("close:" + this->filePath);
        if (!this->isUsable(this->fileMode)) {
            this->hasFailed = true;
            this->closed = true;
            return false;
        }

        this->closed = true;
        if (this->fileSystem.isEnabled(
                this->fileSystem.closeFailures, this->filePath)) {
            this->hasFailed = true;
            return false;
        }
        return true;
    }

    bool failed() const override { return this->hasFailed; }

private:
    bool isUsable(OtaFileMode mode) const {
        return !this->closed &&
               this->fileGeneration == this->fileSystem.generation &&
               this->fileMode == mode;
    }

    static void append(
        std::string& target, SignatureBytes bytes, size_t count) {
        if (count != 0) {
            target.append(reinterpret_cast<const char*>(bytes.data), count);
        }
    }

    FakeOtaFileSystem& fileSystem;
    std::string filePath;
    OtaFileMode fileMode;
    size_t fileGeneration;
    size_t position = 0;
    bool closed = false;
    bool hasFailed = false;
};

OtaFilePresence FakeOtaFileSystem::inspect(const char* path) {
    const std::string filePath(path);
    this->beginOperation("inspect:" + filePath);
    if (this->isEnabled(this->inspectErrors, filePath)) {
        return OtaFilePresence::error;
    }
    return this->files.find(filePath) == this->files.end()
               ? OtaFilePresence::missing
               : OtaFilePresence::present;
}

std::unique_ptr<OtaFile> FakeOtaFileSystem::open(
    const char* path, OtaFileMode mode) {
    const std::string filePath(path);
    const std::string modeName =
        mode == OtaFileMode::read ? "read" : "truncate";
    this->beginOperation("open-" + modeName + ":" + filePath);
    if (this->isEnabled(this->openErrors, filePath) ||
        this->isEnabled(this->inspectErrors, filePath)) {
        return nullptr;
    }

    if (mode == OtaFileMode::read) {
        if (this->readOpenMutationPath == filePath) {
            const auto file = this->files.find(filePath);
            if (file != this->files.end()) {
                const size_t offset =
                    file->second.find(this->readOpenMutationFrom);
                if (offset != std::string::npos) {
                    file->second.replace(
                        offset, this->readOpenMutationFrom.size(),
                        this->readOpenMutationTo);
                    this->didApplyReadOpenMutation = true;
                    this->readOpenMutationPath.clear();
                }
            }
        }
        if (this->files.find(filePath) == this->files.end()) {
            return nullptr;
        }
    } else {
        this->files[filePath].clear();
    }

    return std::make_unique<File>(*this, filePath, mode, this->generation);
}

bool FakeOtaFileSystem::remove(const char* path) {
    const std::string filePath(path);
    this->beginOperation("remove:" + filePath);
    if (this->isEnabled(this->inspectErrors, filePath)) {
        return false;
    }
    this->files.erase(filePath);
    return true;
}

void FakeOtaFileSystem::seed(const char* path, std::string fileContents) {
    this->files[path] = std::move(fileContents);
}

std::optional<std::string> FakeOtaFileSystem::contents(const char* path) const {
    const auto file = this->files.find(path);
    if (file == this->files.end()) {
        return std::nullopt;
    }
    return file->second;
}

size_t FakeOtaFileSystem::bytesRead(const char* path) const {
    const auto count = this->readByteCounts.find(path);
    return count == this->readByteCounts.end() ? 0 : count->second;
}

size_t FakeOtaFileSystem::operations() const {
    return this->operationCount;
}

const std::vector<std::string>& FakeOtaFileSystem::operationLog() const {
    return this->operationHistory;
}

void FakeOtaFileSystem::failBefore(size_t operationIndex) {
    this->failOperation = operationIndex;
}

void FakeOtaFileSystem::tearWriteAfter(size_t bytes) {
    this->tearWriteBytes = bytes;
}

void FakeOtaFileSystem::shortWriteAfter(size_t bytes) {
    this->shortWriteBytes = bytes;
}

void FakeOtaFileSystem::setInspectError(const char* path, bool enabled) {
    this->inspectErrors[path] = enabled;
}

void FakeOtaFileSystem::setOpenError(const char* path, bool enabled) {
    this->openErrors[path] = enabled;
}

void FakeOtaFileSystem::setReadErrorAfter(const char* path, size_t bytes) {
    this->readErrorOffsets[path] = bytes;
}

void FakeOtaFileSystem::setCloseFailure(const char* path, bool enabled) {
    this->closeFailures[path] = enabled;
}

void FakeOtaFileSystem::mutateOnNextReadOpen(
    const char* path, std::string from, std::string to) {
    this->readOpenMutationPath = path;
    this->readOpenMutationFrom = std::move(from);
    this->readOpenMutationTo = std::move(to);
    this->didApplyReadOpenMutation = false;
}

bool FakeOtaFileSystem::readOpenMutationApplied() const {
    return this->didApplyReadOpenMutation;
}

void FakeOtaFileSystem::reboot() {
    ++this->generation;
    this->inspectErrors.clear();
    this->openErrors.clear();
    this->readErrorOffsets.clear();
    this->readByteCounts.clear();
    this->closeFailures.clear();
    this->failOperation.reset();
    this->tearWriteBytes.reset();
    this->shortWriteBytes.reset();
    this->readOpenMutationPath.clear();
    this->readOpenMutationFrom.clear();
    this->readOpenMutationTo.clear();
    this->didApplyReadOpenMutation = false;
}

void FakeOtaFileSystem::beginOperation(const std::string& operation) {
    const size_t operationIndex = this->operationCount++;
    this->operationHistory.push_back(operation);
    if (this->failOperation == operationIndex) {
        this->failOperation.reset();
        throw PowerCut{};
    }
}

bool FakeOtaFileSystem::isEnabled(
    const std::map<std::string, bool>& controls,
    const std::string& path) const {
    const auto control = controls.find(path);
    return control != controls.end() && control->second;
}
