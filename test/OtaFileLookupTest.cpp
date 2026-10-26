#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <optional>
#include <string>

#include "FakeOtaFileSystem.hpp"
#include "common/ConfigBootCoordinator.hpp"
#include "common/OtaFileLookup.hpp"
#include "common/UploadJournal.hpp"
#include "common/UploadStatus.hpp"

namespace {

constexpr int statSuccess = 0;
constexpr int statNotFound = -10002;
constexpr int statIoError = -10003;
constexpr const char* primaryStatusPath = "/upload_status.json";
constexpr const char* backupStatusPath = "/upload_status_backup.json";

class RawStatFakeFileSystem final : public OtaFileSystem {
public:
    explicit RawStatFakeFileSystem(FakeOtaFileSystem& storage)
        : storage(storage) {}

    OtaFilePresence inspect(const char* path) override {
        return classifyOtaStatResult(
            this->rawStatResult(path), statSuccess, statNotFound);
    }

    std::unique_ptr<OtaFile> open(const char* path, OtaFileMode mode) override {
        return this->storage.open(path, mode);
    }

    bool remove(const char* path) override {
        const OtaFilePresence presence = this->inspect(path);
        return removeOtaFileIfPresent(
            presence, [this, path]() { return this->storage.remove(path); });
    }

    void setRawStatResult(const char* path, int result) {
        this->statResults[path] = result;
    }

private:
    int rawStatResult(const char* path) {
        const auto result = this->statResults.find(path);
        if (result != this->statResults.end()) {
            return result->second;
        }
        return this->storage.inspect(path) == OtaFilePresence::present
                   ? statSuccess
                   : statNotFound;
    }

    FakeOtaFileSystem& storage;
    std::map<std::string, int> statResults;
};

UploadStatus makeStatus(
    uint32_t sequence, uint32_t activeVersion, uint32_t minimumVersion) {
    UploadStatus status;
    status.sequence = sequence;
    status.activeVersion = activeVersion;
    status.minimumAcceptedConfigVersion = minimumVersion;
    return status;
}

void seedStatusCopies(FakeOtaFileSystem& storage, const UploadStatus& status) {
    const std::string encoded = encodeUploadStatus(status);
    storage.seed(primaryStatusPath, encoded);
    storage.seed(backupStatusPath, encoded);
}

}  // namespace

TEST(OtaFileLookupTest, ClassifiesOnlyExactSuccessAndNotFoundCodes) {
    EXPECT_EQ(
        classifyOtaStatResult(statSuccess, statSuccess, statNotFound),
        OtaFilePresence::present);
    EXPECT_EQ(
        classifyOtaStatResult(statNotFound, statSuccess, statNotFound),
        OtaFilePresence::missing);
    EXPECT_EQ(
        classifyOtaStatResult(statIoError, statSuccess, statNotFound),
        OtaFilePresence::error);
    EXPECT_EQ(
        classifyOtaStatResult(-1, statSuccess, statNotFound),
        OtaFilePresence::error);
    EXPECT_EQ(
        classifyOtaStatResult(-10004, statSuccess, statNotFound),
        OtaFilePresence::error);
}

TEST(OtaFileLookupTest, RemovalRequiresConfirmedPresenceOrAbsence) {
    size_t removeCalls = 0;
    const auto remove = [&removeCalls]() {
        ++removeCalls;
        return true;
    };

    EXPECT_TRUE(removeOtaFileIfPresent(OtaFilePresence::missing, remove));
    EXPECT_EQ(removeCalls, 0u);
    EXPECT_FALSE(removeOtaFileIfPresent(OtaFilePresence::error, remove));
    EXPECT_EQ(removeCalls, 0u);
    EXPECT_TRUE(removeOtaFileIfPresent(OtaFilePresence::present, remove));
    EXPECT_EQ(removeCalls, 1u);
    EXPECT_FALSE(removeOtaFileIfPresent(
        OtaFilePresence::present, []() { return false; }));
}

TEST(OtaFileLookupTest, ColdBootLookupErrorsCannotInitializeOrRewriteJournal) {
    FakeOtaFileSystem storage;
    const UploadStatus existing = makeStatus(9, 12, 17);
    seedStatusCopies(storage, existing);
    storage.seed(
        "/device_config.json",
        R"({"configVersion":12,"otaEnabled":true,"servicePrefix":"home"})");
    const auto primaryBefore = storage.contents(primaryStatusPath);
    const auto backupBefore = storage.contents(backupStatusPath);

    RawStatFakeFileSystem fileSystem(storage);
    fileSystem.setRawStatResult(primaryStatusPath, statIoError);
    fileSystem.setRawStatResult(backupStatusPath, -1);
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);

    const ConfigBootSelection selection = boot.selectForBoot();

    EXPECT_FALSE(selection.otaAllowed);
    EXPECT_EQ(selection.reason, "status-io-error");
    EXPECT_FALSE(journal.writable());
    EXPECT_EQ(storage.contents(primaryStatusPath), primaryBefore);
    EXPECT_EQ(storage.contents(backupStatusPath), backupBefore);

    fileSystem.setRawStatResult(primaryStatusPath, statSuccess);
    fileSystem.setRawStatResult(backupStatusPath, statSuccess);
    UploadJournal recoveredJournal(fileSystem);
    const JournalRecovery recovered = recoveredJournal.recover();
    ASSERT_EQ(recovered.mode, JournalMode::recovered);
    ASSERT_TRUE(recovered.state.has_value());
    EXPECT_EQ(recovered.state->minimumAcceptedConfigVersion, 17u);
    EXPECT_EQ(recovered.state->sequence, existing.sequence);
}

TEST(OtaFileLookupTest, OneLookupErrorAndOneConfirmedAbsenceAreNotFresh) {
    FakeOtaFileSystem storage;
    RawStatFakeFileSystem fileSystem(storage);
    fileSystem.setRawStatResult(primaryStatusPath, statIoError);
    fileSystem.setRawStatResult(backupStatusPath, statNotFound);

    UploadJournal journal(fileSystem);

    EXPECT_EQ(journal.recover().mode, JournalMode::ioError);
    EXPECT_FALSE(journal.writable());
}

TEST(OtaFileLookupTest, TwoConfirmedAbsencesPermitFreshBootstrap) {
    FakeOtaFileSystem storage;
    RawStatFakeFileSystem fileSystem(storage);
    fileSystem.setRawStatResult(primaryStatusPath, statNotFound);
    fileSystem.setRawStatResult(backupStatusPath, statNotFound);
    UploadJournal journal(fileSystem);

    EXPECT_EQ(journal.recover().mode, JournalMode::fresh);
    ASSERT_TRUE(journal.writable());
    EXPECT_TRUE(journal.store(makeStatus(0, 3, 4)));
    EXPECT_TRUE(storage.contents(primaryStatusPath).has_value());
    EXPECT_TRUE(storage.contents(backupStatusPath).has_value());
}

TEST(OtaFileLookupTest, CleanupLookupErrorDoesNotCallUnderlyingRemove) {
    FakeOtaFileSystem storage;
    storage.seed("/temporary", "contents");
    RawStatFakeFileSystem fileSystem(storage);
    fileSystem.setRawStatResult("/temporary", statIoError);

    EXPECT_FALSE(fileSystem.remove("/temporary"));
    EXPECT_EQ(storage.contents("/temporary"), "contents");
    EXPECT_EQ(
        std::count(
            storage.operationLog().begin(), storage.operationLog().end(),
            "remove:/temporary"),
        0);
}
