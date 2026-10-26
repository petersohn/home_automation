#include "UploadJournal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

#include "UploadStatus.hpp"

namespace {

constexpr const char* primaryPath = "/upload_status.json";
constexpr const char* backupPath = "/upload_status_backup.json";
constexpr std::size_t maxMetadataBytes = 4096;

const char* pathFor(size_t index) {
    return index == 0 ? primaryPath : backupPath;
}

bool sameState(const UploadStatus& left, const UploadStatus& right) {
    const std::string leftEncoded = encodeUploadStatus(left);
    return !leftEncoded.empty() && leftEncoded == encodeUploadStatus(right);
}

bool validCopyOrder(const UploadStatus& left, const UploadStatus& right) {
    if (left.sequence == right.sequence) {
        return sameState(left, right);
    }

    const UploadStatus& older = left.sequence < right.sequence ? left : right;
    const UploadStatus& newer = left.sequence < right.sequence ? right : left;
    if (newer.sequence - older.sequence != 1 ||
        newer.activeVersion < older.activeVersion) {
        return false;
    }

    const uint32_t olderFloor = older.minimumAcceptedConfigVersion.value_or(
        std::numeric_limits<uint32_t>::max());
    const uint32_t newerFloor = newer.minimumAcceptedConfigVersion.value_or(
        std::numeric_limits<uint32_t>::max());
    return newerFloor >= olderFloor;
}

void mergeFloor(
    std::optional<uint32_t>& destination,
    const std::optional<uint32_t>& candidate) {
    if (!destination.has_value() || !candidate.has_value()) {
        destination.reset();
    } else {
        destination = std::max(*destination, *candidate);
    }
}

bool floorDoesNotRegress(
    const std::optional<uint32_t>& current,
    const std::optional<uint32_t>& desired) {
    if (!current.has_value()) {
        return !desired.has_value();
    }
    return !desired.has_value() || *desired >= *current;
}

struct FileRead {
    enum class Kind { ioError, invalid, valid };

    Kind kind = Kind::ioError;
    std::string contents;
};

FileRead readOpenedFile(OtaFile& file) {
    FileRead result;
    bool tooLarge = false;
    bool readFailed = false;

    while (result.contents.size() <= maxMetadataBytes) {
        const int value = file.read();
        if (value == -1) {
            break;
        }
        if (value < 0 || value > 255) {
            readFailed = true;
            break;
        }
        result.contents.push_back(static_cast<char>(value));
        if (result.contents.size() > maxMetadataBytes) {
            tooLarge = true;
            break;
        }
    }

    readFailed = readFailed || file.failed();
    const bool closeSucceeded = file.close();
    const bool closeFailed = file.failed();
    if (readFailed || !closeSucceeded || closeFailed) {
        result.kind = FileRead::Kind::ioError;
    } else if (tooLarge) {
        result.kind = FileRead::Kind::invalid;
    } else {
        result.kind = FileRead::Kind::valid;
    }
    return result;
}

}  // namespace

UploadJournal::UploadJournal(OtaFileSystem& fileSystem)
    : fileSystem(fileSystem) {}

UploadJournal::CopyRecord UploadJournal::readCopy(const char* path) {
    CopyRecord result;
    const OtaFilePresence presence = this->fileSystem.inspect(path);
    if (presence == OtaFilePresence::missing) {
        result.kind = CopyRecord::Kind::missing;
        return result;
    }
    if (presence == OtaFilePresence::error) {
        result.kind = CopyRecord::Kind::ioError;
        return result;
    }

    std::unique_ptr<OtaFile> file =
        this->fileSystem.open(path, OtaFileMode::read);
    if (file == nullptr) {
        result.kind = CopyRecord::Kind::ioError;
        return result;
    }

    FileRead read = readOpenedFile(*file);
    if (read.kind == FileRead::Kind::ioError) {
        result.kind = CopyRecord::Kind::ioError;
        return result;
    }
    if (read.kind == FileRead::Kind::invalid) {
        result.kind = CopyRecord::Kind::invalid;
        return result;
    }

    UploadStatus decoded;
    if (!decodeUploadStatus(read.contents, decoded)) {
        result.kind = CopyRecord::Kind::invalid;
        return result;
    }
    result.kind = CopyRecord::Kind::valid;
    result.state = decoded;
    return result;
}

JournalRecovery UploadJournal::recover() {
    const std::optional<UploadStatus> previousState = this->currentState;
    this->copies[0] = this->readCopy(primaryPath);
    this->copies[1] = this->readCopy(backupPath);
    this->recoveryComplete = true;
    this->needsRecovery = false;

    const bool ioError = this->copies[0].kind == CopyRecord::Kind::ioError ||
                         this->copies[1].kind == CopyRecord::Kind::ioError;
    const bool primaryValid = this->copies[0].kind == CopyRecord::Kind::valid;
    const bool backupValid = this->copies[1].kind == CopyRecord::Kind::valid;

    if (primaryValid && backupValid &&
        !validCopyOrder(*this->copies[0].state, *this->copies[1].state)) {
        this->canStore = false;
        this->isWritable = false;
        return {JournalMode::corrupt, previousState};
    }

    if (primaryValid || backupValid) {
        UploadStatus selected =
            primaryValid ? *this->copies[0].state : *this->copies[1].state;
        if (primaryValid && backupValid &&
            this->copies[1].state->sequence > selected.sequence) {
            selected = *this->copies[1].state;
        }

        if (primaryValid) {
            mergeFloor(
                selected.minimumAcceptedConfigVersion,
                this->copies[0].state->minimumAcceptedConfigVersion);
        }
        if (backupValid) {
            mergeFloor(
                selected.minimumAcceptedConfigVersion,
                this->copies[1].state->minimumAcceptedConfigVersion);
        }

        if (encodeUploadStatus(selected).empty()) {
            this->currentState = previousState;
            this->canStore = false;
            this->isWritable = false;
            return {JournalMode::corrupt, this->currentState};
        }

        this->currentState = selected;
        const std::string canonical = encodeUploadStatus(selected);
        const bool primaryAuthoritative =
            primaryValid &&
            encodeUploadStatus(*this->copies[0].state) == canonical;
        const bool backupAuthoritative =
            backupValid &&
            encodeUploadStatus(*this->copies[1].state) == canonical;
        this->isWritable = !ioError && primaryValid && backupValid &&
                           primaryAuthoritative && backupAuthoritative;
        this->canStore =
            !ioError && (primaryAuthoritative || backupAuthoritative);
        return {JournalMode::recovered, this->currentState};
    }

    if (previousState.has_value()) {
        this->currentState = previousState;
        this->canStore = false;
        this->isWritable = false;
        if (ioError) {
            return {JournalMode::ioError, this->currentState};
        }
        return {JournalMode::recovered, this->currentState};
    }

    this->currentState.reset();
    this->canStore = false;
    this->isWritable = false;
    if (ioError) {
        return {JournalMode::ioError, std::nullopt};
    }
    if (this->copies[0].kind == CopyRecord::Kind::missing &&
        this->copies[1].kind == CopyRecord::Kind::missing) {
        this->canStore = true;
        this->isWritable = true;
        return {JournalMode::fresh, std::nullopt};
    }
    return {JournalMode::corrupt, std::nullopt};
}

bool UploadJournal::writeAndVerify(size_t index, const UploadStatus& desired) {
    const std::string expected = encodeUploadStatus(desired);
    if (expected.empty()) {
        return false;
    }

    std::unique_ptr<OtaFile> file =
        this->fileSystem.open(pathFor(index), OtaFileMode::truncate);
    if (file == nullptr) {
        return false;
    }

    const size_t written = file->write(
        SignatureBytes{
            reinterpret_cast<const uint8_t*>(expected.data()),
            expected.size()});
    const bool writeSucceeded = written == expected.size() && !file->failed();
    const bool closeSucceeded = file->close();
    const bool closeFailed = file->failed();
    if (!writeSucceeded || !closeSucceeded || closeFailed) {
        return false;
    }

    std::unique_ptr<OtaFile> verifyFile =
        this->fileSystem.open(pathFor(index), OtaFileMode::read);
    if (verifyFile == nullptr) {
        return false;
    }
    const FileRead read = readOpenedFile(*verifyFile);
    if (read.kind != FileRead::Kind::valid) {
        return false;
    }

    UploadStatus verified;
    if (!decodeUploadStatus(read.contents, verified) ||
        !sameState(verified, desired)) {
        return false;
    }
    this->copies[index].kind = CopyRecord::Kind::valid;
    this->copies[index].state = verified;
    return true;
}

bool UploadJournal::store(const UploadStatus& desired) {
    if (!this->recoveryComplete || !this->canStore || this->needsRecovery) {
        return false;
    }

    UploadStatus next = desired;
    if (this->currentState.has_value()) {
        if (this->currentState->sequence ==
                std::numeric_limits<uint32_t>::max() ||
            !floorDoesNotRegress(
                this->currentState->minimumAcceptedConfigVersion,
                desired.minimumAcceptedConfigVersion)) {
            return false;
        }
        next.sequence = this->currentState->sequence + 1;
    } else {
        next.sequence = 1;
    }
    if (encodeUploadStatus(next).empty()) {
        return false;
    }

    size_t firstTarget = 0;
    const bool primaryValid = this->copies[0].kind == CopyRecord::Kind::valid;
    const bool backupValid = this->copies[1].kind == CopyRecord::Kind::valid;
    if (primaryValid && backupValid) {
        if (this->copies[0].state->sequence <=
            this->copies[1].state->sequence) {
            firstTarget = 0;
        } else {
            firstTarget = 1;
        }
    } else if (primaryValid) {
        firstTarget = 1;
    } else if (backupValid) {
        firstTarget = 0;
    }

    if (!this->writeAndVerify(firstTarget, next)) {
        this->copies[firstTarget] = CopyRecord{};
        this->canStore = false;
        this->isWritable = false;
        this->needsRecovery = true;
        return false;
    }

    this->currentState = next;
    this->isWritable = false;
    if (!this->mirrorCurrent()) {
        this->canStore = false;
        this->isWritable = false;
        this->needsRecovery = true;
        return false;
    }
    return true;
}

bool UploadJournal::mirrorCurrent() {
    if (!this->currentState.has_value()) {
        this->canStore = false;
        this->isWritable = false;
        this->needsRecovery = true;
        return false;
    }

    const std::string canonical = encodeUploadStatus(*this->currentState);
    if (canonical.empty()) {
        this->canStore = false;
        this->isWritable = false;
        this->needsRecovery = true;
        return false;
    }
    const bool primaryCurrent =
        this->copies[0].kind == CopyRecord::Kind::valid &&
        encodeUploadStatus(*this->copies[0].state) == canonical;
    const bool backupCurrent =
        this->copies[1].kind == CopyRecord::Kind::valid &&
        encodeUploadStatus(*this->copies[1].state) == canonical;
    if (primaryCurrent && backupCurrent) {
        this->isWritable = true;
        this->canStore = true;
        this->needsRecovery = false;
        return true;
    }

    if (!primaryCurrent && !backupCurrent) {
        this->canStore = false;
        this->isWritable = false;
        this->needsRecovery = true;
        return false;
    }

    const size_t firstTarget = primaryCurrent ? 1 : 0;

    if (!this->writeAndVerify(firstTarget, *this->currentState)) {
        this->copies[firstTarget] = CopyRecord{};
        this->canStore = false;
        this->isWritable = false;
        this->needsRecovery = true;
        return false;
    }

    this->isWritable = true;
    this->canStore = true;
    this->needsRecovery = false;
    return true;
}

bool UploadJournal::mirror() {
    const JournalRecovery recovery = this->recover();
    if (recovery.mode != JournalMode::recovered || !this->canStore ||
        !this->currentState.has_value()) {
        this->canStore = false;
        this->isWritable = false;
        this->needsRecovery = true;
        return false;
    }
    const bool success = this->mirrorCurrent();
    if (!success) {
        this->canStore = false;
        this->isWritable = false;
        this->needsRecovery = true;
    }
    return success;
}

const std::optional<UploadStatus>& UploadJournal::state() const {
    return this->currentState;
}

bool UploadJournal::writable() const {
    return this->isWritable;
}
