#ifndef COMMON_UPLOAD_JOURNAL_HPP
#define COMMON_UPLOAD_JOURNAL_HPP

#include <array>
#include <cstddef>
#include <optional>

#include "OtaFile.hpp"
#include "OtaTypes.hpp"

enum class JournalMode { recovered, fresh, corrupt, ioError };

struct JournalRecovery {
    JournalMode mode;
    std::optional<UploadStatus> state;
};

class UploadJournal {
public:
    explicit UploadJournal(OtaFileSystem& fileSystem);

    JournalRecovery recover();
    bool store(const UploadStatus& desired);
    bool mirror();
    const std::optional<UploadStatus>& state() const;
    bool writable() const;

private:
    struct CopyRecord {
        enum class Kind { unknown, missing, invalid, ioError, valid };

        Kind kind = Kind::unknown;
        std::optional<UploadStatus> state;
    };

    OtaFileSystem& fileSystem;
    std::array<CopyRecord, 2> copies;
    std::optional<UploadStatus> currentState;
    bool recoveryComplete = false;
    bool canStore = false;
    bool isWritable = false;
    bool needsRecovery = false;

    CopyRecord readCopy(const char* path);
    bool writeAndVerify(size_t index, const UploadStatus& desired);
    bool mirrorCurrent();
};

#endif  // COMMON_UPLOAD_JOURNAL_HPP
