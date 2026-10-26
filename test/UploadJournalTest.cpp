#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "FakeOtaFileSystem.hpp"
#include "common/ConfigBootCoordinator.hpp"
#include "common/OtaTypes.hpp"
#include "common/UploadJournal.hpp"
#include "common/UploadStatus.hpp"

namespace {

constexpr const char* primaryPath = "/upload_status.json";
constexpr const char* backupPath = "/upload_status_backup.json";
constexpr const char* primaryConfigPath = "/device_config.json";
constexpr const char* secondaryConfigPath = "/device_config_2.json";

UploadStatus makeIdle(
    uint32_t sequence, ConfigSlot activeSlot, uint32_t activeVersion,
    std::optional<uint32_t> minimum) {
    UploadStatus status;
    status.sequence = sequence;
    status.activeSlot = activeSlot;
    status.activeVersion = activeVersion;
    status.minimumAcceptedConfigVersion = minimum;
    return status;
}

void seedRecord(
    FakeOtaFileSystem& fileSystem, const char* path,
    const UploadStatus& status) {
    fileSystem.seed(path, encodeUploadStatus(status));
}

std::optional<UploadStatus> decodedContents(
    const FakeOtaFileSystem& fileSystem, const char* path) {
    const auto contents = fileSystem.contents(path);
    if (!contents.has_value()) {
        return std::nullopt;
    }
    UploadStatus status;
    if (!decodeUploadStatus(*contents, status)) {
        return std::nullopt;
    }
    return status;
}

UploadStatus expectRecovered(
    FakeOtaFileSystem& fileSystem, ConfigSlot expectedSlot,
    uint32_t expectedFloor) {
    UploadJournal journal(fileSystem);
    const JournalRecovery recovered = journal.recover();
    EXPECT_EQ(recovered.mode, JournalMode::recovered);
    EXPECT_TRUE(recovered.state.has_value());
    if (!recovered.state.has_value()) {
        return UploadStatus{};
    }
    EXPECT_EQ(recovered.state->activeSlot, expectedSlot);
    EXPECT_TRUE(recovered.state->minimumAcceptedConfigVersion.has_value());
    if (!recovered.state->minimumAcceptedConfigVersion.has_value()) {
        return *recovered.state;
    }
    EXPECT_GE(*recovered.state->minimumAcceptedConfigVersion, expectedFloor);
    return *recovered.state;
}

void expectFloorAtLeast(const UploadStatus& status, uint32_t expectedFloor) {
    ASSERT_TRUE(status.minimumAcceptedConfigVersion.has_value());
    EXPECT_GE(*status.minimumAcceptedConfigVersion, expectedFloor);
}

void expectBoundaryPreserved(
    const UploadStatus& status,
    const std::optional<uint32_t>& expectedBoundary) {
    if (!expectedBoundary.has_value()) {
        EXPECT_FALSE(status.minimumAcceptedConfigVersion.has_value());
        return;
    }
    expectFloorAtLeast(status, *expectedBoundary);
}

std::size_t mutationCount(const FakeOtaFileSystem& fileSystem) {
    const auto& operations = fileSystem.operationLog();
    return static_cast<std::size_t>(std::count_if(
        operations.begin(), operations.end(), [](const std::string& operation) {
        return operation.rfind("open-truncate:", 0) == 0 ||
               operation.rfind("write:", 0) == 0 ||
               operation.rfind("remove:", 0) == 0;
    }));
}

struct JournalCopies {
    std::optional<UploadStatus> primary;
    std::optional<UploadStatus> backup;
};

void seedCopies(FakeOtaFileSystem& fileSystem, const JournalCopies& copies) {
    if (copies.primary.has_value()) {
        seedRecord(fileSystem, primaryPath, *copies.primary);
    }
    if (copies.backup.has_value()) {
        seedRecord(fileSystem, backupPath, *copies.backup);
    }
}

std::string makeDeviceConfig(uint32_t version) {
    return R"({"otaEnabled":true,"configVersion":)" + std::to_string(version) +
           R"(,"servicePrefix":"home/device"})";
}

// Distinct mirrored records must be adjacent; version and floor cannot regress.
void expectInvalidJournalOrder(
    const UploadStatus& older, const UploadStatus& newer, bool olderInPrimary,
    uint32_t strongestFloor) {
    const UploadStatus& primary = olderInPrimary ? older : newer;
    const UploadStatus& backup = olderInPrimary ? newer : older;
    FakeOtaFileSystem fileSystem;
    seedRecord(fileSystem, primaryPath, primary);
    seedRecord(fileSystem, backupPath, backup);
    const auto originalPrimary = fileSystem.contents(primaryPath);
    const auto originalBackup = fileSystem.contents(backupPath);
    ASSERT_TRUE(decodedContents(fileSystem, primaryPath).has_value());
    ASSERT_TRUE(decodedContents(fileSystem, backupPath).has_value());

    UploadJournal journal(fileSystem);
    EXPECT_EQ(journal.recover().mode, JournalMode::corrupt);
    EXPECT_FALSE(journal.writable());
    const size_t mutationsAfterRecovery = mutationCount(fileSystem);
    EXPECT_FALSE(journal.mirror());
    EXPECT_FALSE(journal.store(makeIdle(0, ConfigSlot::primary, 9, 10)));
    EXPECT_EQ(mutationCount(fileSystem), mutationsAfterRecovery);
    EXPECT_EQ(fileSystem.contents(primaryPath), originalPrimary);
    EXPECT_EQ(fileSystem.contents(backupPath), originalBackup);
    const auto preservedPrimary = decodedContents(fileSystem, primaryPath);
    const auto preservedBackup = decodedContents(fileSystem, backupPath);
    ASSERT_TRUE(preservedPrimary.has_value());
    ASSERT_TRUE(preservedBackup.has_value());
    ASSERT_TRUE(preservedPrimary->minimumAcceptedConfigVersion.has_value());
    ASSERT_TRUE(preservedBackup->minimumAcceptedConfigVersion.has_value());
    EXPECT_EQ(
        std::max(
            *preservedPrimary->minimumAcceptedConfigVersion,
            *preservedBackup->minimumAcceptedConfigVersion),
        strongestFloor);

    FakeOtaFileSystem bootFileSystem;
    seedRecord(bootFileSystem, primaryPath, primary);
    seedRecord(bootFileSystem, backupPath, backup);
    bootFileSystem.seed(
        primaryConfigPath, makeDeviceConfig(older.activeVersion));
    bootFileSystem.seed(
        secondaryConfigPath, makeDeviceConfig(newer.activeVersion));
    UploadJournal bootJournal(bootFileSystem);
    ConfigBootCoordinator boot(bootFileSystem, bootJournal);
    const ConfigBootSelection selection = boot.selectForBoot();
    EXPECT_FALSE(selection.otaAllowed);
    EXPECT_FALSE(boot.snapshot().updatesAllowed);
    EXPECT_FALSE(bootJournal.writable());
    EXPECT_EQ(
        bootFileSystem.contents(primaryPath),
        std::optional<std::string>{encodeUploadStatus(primary)});
    EXPECT_EQ(
        bootFileSystem.contents(backupPath),
        std::optional<std::string>{encodeUploadStatus(backup)});
}

}  // namespace

TEST(UploadJournalTest, BackupSurvivesPrimaryTruncate) {
    FakeOtaFileSystem fileSystem;
    const UploadStatus stale = makeIdle(8, ConfigSlot::primary, 10, 13);
    const UploadStatus backup = makeIdle(9, ConfigSlot::secondary, 12, 13);
    seedRecord(fileSystem, primaryPath, stale);
    seedRecord(fileSystem, backupPath, backup);

    UploadJournal journal(fileSystem);
    ASSERT_EQ(journal.recover().mode, JournalMode::recovered);
    const UploadStatus desired = makeIdle(0, ConfigSlot::secondary, 12, 14);
    ASSERT_TRUE(journal.store(desired));

    const auto& history = fileSystem.operationLog();
    const auto truncate = std::find(
        history.begin(), history.end(),
        std::string("open-truncate:") + primaryPath);
    ASSERT_NE(truncate, history.end());
    const size_t truncateIndex =
        static_cast<size_t>(std::distance(history.begin(), truncate));

    FakeOtaFileSystem interrupted;
    seedRecord(interrupted, primaryPath, stale);
    seedRecord(interrupted, backupPath, backup);
    UploadJournal interruptedJournal(interrupted);
    ASSERT_EQ(interruptedJournal.recover().mode, JournalMode::recovered);
    interrupted.failBefore(truncateIndex + 1);

    EXPECT_THROW(
        interruptedJournal.store(desired), FakeOtaFileSystem::PowerCut);
    EXPECT_EQ(
        interrupted.contents(primaryPath), std::optional<std::string>{""});
    EXPECT_EQ(
        interrupted.contents(backupPath),
        std::optional<std::string>{encodeUploadStatus(backup)});

    interrupted.reboot();
    const UploadStatus recovered =
        expectRecovered(interrupted, ConfigSlot::secondary, 13);
    EXPECT_EQ(recovered.sequence, backup.sequence);
}

TEST(UploadJournalTest, NewerFloorRegressionIsCorrupt) {
    const UploadStatus older = makeIdle(7, ConfigSlot::primary, 10, 14);
    const UploadStatus newer = makeIdle(8, ConfigSlot::secondary, 12, 13);
    for (const bool olderInPrimary : {true, false}) {
        SCOPED_TRACE(olderInPrimary);
        expectInvalidJournalOrder(older, newer, olderInPrimary, 14);
    }
}

TEST(UploadJournalTest, ExhaustionWinsOverStaleMinimum) {
    FakeOtaFileSystem fileSystem;
    seedRecord(
        fileSystem, primaryPath, makeIdle(13, ConfigSlot::primary, 10, 13));
    seedRecord(
        fileSystem, backupPath,
        makeIdle(14, ConfigSlot::secondary, UINT32_MAX, std::nullopt));

    UploadJournal journal(fileSystem);
    const JournalRecovery recovered = journal.recover();

    ASSERT_EQ(recovered.mode, JournalMode::recovered);
    ASSERT_TRUE(recovered.state.has_value());
    EXPECT_EQ(recovered.state->sequence, 14u);
    EXPECT_EQ(recovered.state->activeSlot, ConfigSlot::secondary);
    EXPECT_FALSE(recovered.state->minimumAcceptedConfigVersion.has_value());
}

TEST(UploadJournalTest, BothMissingDiffersFromCorrupt) {
    FakeOtaFileSystem missing;
    UploadJournal missingJournal(missing);
    EXPECT_EQ(missingJournal.recover().mode, JournalMode::fresh);

    FakeOtaFileSystem corrupt;
    corrupt.seed(primaryPath, "not a status record");
    corrupt.seed(backupPath, "also not a status record");
    UploadJournal corruptJournal(corrupt);
    EXPECT_EQ(corruptJournal.recover().mode, JournalMode::corrupt);
}

TEST(UploadJournalTest, ReadErrorNeverLooksFresh) {
    FakeOtaFileSystem fileSystem;
    seedRecord(
        fileSystem, primaryPath, makeIdle(2, ConfigSlot::secondary, 12, 13));
    fileSystem.setReadErrorAfter(primaryPath, 0);

    UploadJournal journal(fileSystem);
    EXPECT_EQ(journal.recover().mode, JournalMode::ioError);

    FakeOtaFileSystem inspectError;
    inspectError.setInspectError(primaryPath);
    UploadJournal inspectErrorJournal(inspectError);
    EXPECT_EQ(inspectErrorJournal.recover().mode, JournalMode::ioError);
}

TEST(UploadJournalTest, ValidBackupNeverDefaultsToPrimary) {
    FakeOtaFileSystem fileSystem;
    const UploadStatus backup = makeIdle(5, ConfigSlot::secondary, 12, 13);
    seedRecord(fileSystem, backupPath, backup);

    const UploadStatus recovered =
        expectRecovered(fileSystem, ConfigSlot::secondary, 13);
    EXPECT_EQ(recovered.sequence, backup.sequence);
}

TEST(UploadJournalTest, SameSequenceConflictsAreCorrupt) {
    FakeOtaFileSystem fileSystem;
    seedRecord(
        fileSystem, primaryPath, makeIdle(6, ConfigSlot::primary, 10, 13));
    seedRecord(
        fileSystem, backupPath, makeIdle(6, ConfigSlot::secondary, 12, 13));

    UploadJournal journal(fileSystem);
    EXPECT_EQ(journal.recover().mode, JournalMode::corrupt);
}

TEST(UploadJournalTest, NonAdjacentSequencesAreCorruptInBothCopyOrientations) {
    const UploadStatus older = makeIdle(2, ConfigSlot::primary, 10, 13);
    const UploadStatus newer = makeIdle(4, ConfigSlot::secondary, 12, 13);
    for (const bool olderInPrimary : {true, false}) {
        SCOPED_TRACE(olderInPrimary);
        expectInvalidJournalOrder(older, newer, olderInPrimary, 13);
    }
}

TEST(
    UploadJournalTest, NewerActiveVersionCannotRegressInEitherCopyOrientation) {
    const UploadStatus older = makeIdle(7, ConfigSlot::primary, 12, 13);
    const UploadStatus newer = makeIdle(8, ConfigSlot::secondary, 11, 13);
    for (const bool olderInPrimary : {true, false}) {
        SCOPED_TRACE(olderInPrimary);
        expectInvalidJournalOrder(older, newer, olderInPrimary, 13);
    }
}

TEST(UploadJournalTest, SequenceWrapIsCorruptInBothCopyOrientations) {
    const UploadStatus maxSequence =
        makeIdle(UINT32_MAX, ConfigSlot::primary, 12, 13);
    const UploadStatus zeroSequence =
        makeIdle(0, ConfigSlot::secondary, 12, 13);
    for (const bool maxInPrimary : {true, false}) {
        SCOPED_TRACE(maxInPrimary);
        const UploadStatus& primary = maxInPrimary ? maxSequence : zeroSequence;
        const UploadStatus& backup = maxInPrimary ? zeroSequence : maxSequence;
        FakeOtaFileSystem fileSystem;
        seedRecord(fileSystem, primaryPath, primary);
        seedRecord(fileSystem, backupPath, backup);
        const auto originalPrimary = fileSystem.contents(primaryPath);
        const auto originalBackup = fileSystem.contents(backupPath);
        UploadJournal journal(fileSystem);

        EXPECT_EQ(journal.recover().mode, JournalMode::corrupt);
        EXPECT_FALSE(journal.writable());
        EXPECT_FALSE(journal.mirror());
        EXPECT_EQ(fileSystem.contents(primaryPath), originalPrimary);
        EXPECT_EQ(fileSystem.contents(backupPath), originalBackup);
    }
}

TEST(UploadJournalTest, StoreRejectsFloorRegressionWithoutChangingFiles) {
    FakeOtaFileSystem fileSystem;
    const UploadStatus current = makeIdle(7, ConfigSlot::secondary, 12, 14);
    seedRecord(fileSystem, backupPath, current);
    UploadJournal journal(fileSystem);
    ASSERT_EQ(journal.recover().mode, JournalMode::recovered);
    const auto oldPrimary = fileSystem.contents(primaryPath);
    const auto oldBackup = fileSystem.contents(backupPath);

    EXPECT_FALSE(journal.store(makeIdle(0, ConfigSlot::secondary, 12, 13)));
    EXPECT_EQ(fileSystem.contents(primaryPath), oldPrimary);
    EXPECT_EQ(fileSystem.contents(backupPath), oldBackup);
    ASSERT_TRUE(journal.state().has_value());
    EXPECT_EQ(
        journal.state()->minimumAcceptedConfigVersion,
        std::optional<uint32_t>{14});
}

TEST(UploadJournalTest, SequenceExhaustionStopsWritesWithoutOverflow) {
    FakeOtaFileSystem fileSystem;
    const UploadStatus maximum =
        makeIdle(UINT32_MAX, ConfigSlot::secondary, UINT32_MAX, std::nullopt);
    seedRecord(fileSystem, backupPath, maximum);
    UploadJournal journal(fileSystem);
    ASSERT_EQ(journal.recover().mode, JournalMode::recovered);
    const auto before = fileSystem.contents(backupPath);

    EXPECT_FALSE(journal.store(maximum));
    EXPECT_EQ(fileSystem.contents(backupPath), before);
    ASSERT_TRUE(journal.state().has_value());
    EXPECT_EQ(journal.state()->sequence, UINT32_MAX);
}

TEST(UploadJournalTest, ReadbackFailureDoesNotLoseRecoverableNewRecord) {
    FakeOtaFileSystem fileSystem;
    const UploadStatus oldState = makeIdle(3, ConfigSlot::secondary, 12, 13);
    seedRecord(fileSystem, backupPath, oldState);
    UploadJournal journal(fileSystem);
    ASSERT_EQ(journal.recover().mode, JournalMode::recovered);
    fileSystem.setReadErrorAfter(primaryPath, 0);

    EXPECT_FALSE(journal.store(makeIdle(0, ConfigSlot::secondary, 12, 14)));
    EXPECT_FALSE(journal.writable());
    const auto written = decodedContents(fileSystem, primaryPath);
    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(written->activeSlot, ConfigSlot::secondary);
    expectFloorAtLeast(*written, 14);

    fileSystem.reboot();
    const UploadStatus recovered =
        expectRecovered(fileSystem, ConfigSlot::secondary, 14);
    EXPECT_GT(recovered.sequence, oldState.sequence);
}

TEST(UploadJournalTest, MirrorFailureKeepsValidatedAuthorityAndBackup) {
    FakeOtaFileSystem fileSystem;
    const UploadStatus oldState = makeIdle(3, ConfigSlot::secondary, 12, 13);
    seedRecord(fileSystem, backupPath, oldState);
    UploadJournal journal(fileSystem);
    ASSERT_EQ(journal.recover().mode, JournalMode::recovered);
    fileSystem.setCloseFailure(backupPath);

    EXPECT_FALSE(journal.store(makeIdle(0, ConfigSlot::secondary, 12, 14)));
    EXPECT_FALSE(journal.writable());
    ASSERT_TRUE(journal.state().has_value());
    EXPECT_GT(journal.state()->sequence, oldState.sequence);
    expectFloorAtLeast(*journal.state(), 14);
    EXPECT_TRUE(decodedContents(fileSystem, primaryPath).has_value());

    fileSystem.setCloseFailure(backupPath, false);
    ASSERT_TRUE(journal.mirror());
    EXPECT_TRUE(journal.writable());

    fileSystem.reboot();
    const UploadStatus recovered =
        expectRecovered(fileSystem, ConfigSlot::secondary, 14);
    EXPECT_GT(recovered.sequence, oldState.sequence);
}

TEST(UploadJournalTest, MirrorRepairsMissingCopyWithoutAdvancingSequence) {
    FakeOtaFileSystem fileSystem;
    const UploadStatus current = makeIdle(9, ConfigSlot::secondary, 12, 13);
    seedRecord(fileSystem, backupPath, current);
    UploadJournal journal(fileSystem);
    ASSERT_EQ(journal.recover().mode, JournalMode::recovered);

    ASSERT_TRUE(journal.mirror());
    const auto primary = decodedContents(fileSystem, primaryPath);
    const auto backup = decodedContents(fileSystem, backupPath);
    ASSERT_TRUE(primary.has_value());
    ASSERT_TRUE(backup.has_value());
    EXPECT_EQ(primary->sequence, current.sequence);
    EXPECT_EQ(backup->sequence, current.sequence);
    EXPECT_EQ(primary->activeSlot, ConfigSlot::secondary);
    EXPECT_EQ(backup->activeSlot, ConfigSlot::secondary);
}

TEST(UploadJournalTest, PartialWriteLeavesConfirmedBackupRecoverable) {
    FakeOtaFileSystem fileSystem;
    const UploadStatus oldState = makeIdle(3, ConfigSlot::secondary, 12, 13);
    const std::string backupBytes = encodeUploadStatus(oldState);
    seedRecord(fileSystem, backupPath, oldState);
    UploadJournal journal(fileSystem);
    ASSERT_EQ(journal.recover().mode, JournalMode::recovered);
    fileSystem.tearWriteAfter(8);

    EXPECT_THROW(
        journal.store(makeIdle(0, ConfigSlot::secondary, 12, 14)),
        FakeOtaFileSystem::PowerCut);
    const auto partial = fileSystem.contents(primaryPath);
    ASSERT_TRUE(partial.has_value());
    EXPECT_EQ(partial->size(), 8u);
    EXPECT_EQ(
        fileSystem.contents(backupPath),
        std::optional<std::string>{backupBytes});

    fileSystem.reboot();
    const UploadStatus recovered =
        expectRecovered(fileSystem, ConfigSlot::secondary, 13);
    EXPECT_EQ(recovered.sequence, oldState.sequence);
}

TEST(UploadJournalTest, StoreFaultSweepPreservesLatestConfirmedSlotAndFloor) {
    const UploadStatus confirmed = makeIdle(11, ConfigSlot::secondary, 12, 13);
    const UploadStatus desired = makeIdle(0, ConfigSlot::secondary, 12, 14);

    FakeOtaFileSystem baseline;
    baseline.seed(primaryPath, "partial old primary");
    seedRecord(baseline, backupPath, confirmed);
    UploadJournal baselineJournal(baseline);
    ASSERT_EQ(baselineJournal.recover().mode, JournalMode::recovered);
    const size_t storeStart = baseline.operations();
    ASSERT_TRUE(baselineJournal.store(desired));
    const size_t storeOperations = baseline.operations() - storeStart;
    ASSERT_GT(storeOperations, 0u);

    for (size_t offset = 0; offset < storeOperations; ++offset) {
        SCOPED_TRACE(offset);
        FakeOtaFileSystem fileSystem;
        fileSystem.seed(primaryPath, "partial old primary");
        seedRecord(fileSystem, backupPath, confirmed);
        UploadJournal journal(fileSystem);
        ASSERT_EQ(journal.recover().mode, JournalMode::recovered);
        fileSystem.failBefore(fileSystem.operations() + offset);

        bool interrupted = false;
        try {
            journal.store(desired);
        } catch (const FakeOtaFileSystem::PowerCut&) {
            interrupted = true;
        }
        ASSERT_TRUE(interrupted);

        fileSystem.reboot();
        const auto validPrimary = decodedContents(fileSystem, primaryPath);
        const auto validBackup = decodedContents(fileSystem, backupPath);
        ASSERT_TRUE(validPrimary.has_value() || validBackup.has_value());
        const UploadStatus recovered =
            expectRecovered(fileSystem, ConfigSlot::secondary, 13);

        uint32_t strongestFloor = 13;
        for (const auto& persisted : {validPrimary, validBackup}) {
            if (persisted.has_value() &&
                persisted->minimumAcceptedConfigVersion.has_value()) {
                strongestFloor = std::max(
                    strongestFloor, *persisted->minimumAcceptedConfigVersion);
            }
        }
        expectFloorAtLeast(recovered, strongestFloor);
    }
}

TEST(UploadJournalTest, FloorRegressionCannotProduceRepairableState) {
    const UploadStatus older = makeIdle(7, ConfigSlot::primary, 10, 14);
    const UploadStatus newer = makeIdle(8, ConfigSlot::secondary, 12, 13);
    for (const bool olderInPrimary : {true, false}) {
        SCOPED_TRACE(olderInPrimary);
        expectInvalidJournalOrder(older, newer, olderInPrimary, 14);
    }
}

TEST(UploadJournalTest, OlderExhaustionCannotBeDiscarded) {
    const UploadStatus exhausted =
        makeIdle(7, ConfigSlot::primary, UINT32_MAX, std::nullopt);
    const UploadStatus newer = makeIdle(8, ConfigSlot::secondary, 12, 13);
    for (const bool olderInPrimary : {true, false}) {
        SCOPED_TRACE(olderInPrimary);
        const UploadStatus& primary = olderInPrimary ? exhausted : newer;
        const UploadStatus& backup = olderInPrimary ? newer : exhausted;
        FakeOtaFileSystem fileSystem;
        seedRecord(fileSystem, primaryPath, primary);
        seedRecord(fileSystem, backupPath, backup);
        const auto originalPrimary = fileSystem.contents(primaryPath);
        const auto originalBackup = fileSystem.contents(backupPath);
        UploadJournal journal(fileSystem);

        EXPECT_EQ(journal.recover().mode, JournalMode::corrupt);
        EXPECT_FALSE(journal.writable());
        const size_t mutationsAfterRecovery = mutationCount(fileSystem);
        EXPECT_FALSE(journal.mirror());
        EXPECT_FALSE(journal.store(
            makeIdle(0, ConfigSlot::secondary, UINT32_MAX, std::nullopt)));
        EXPECT_EQ(mutationCount(fileSystem), mutationsAfterRecovery);
        EXPECT_EQ(fileSystem.contents(primaryPath), originalPrimary);
        EXPECT_EQ(fileSystem.contents(backupPath), originalBackup);
    }
}

TEST(UploadJournalTest, PublicMirrorFailureBlocksStore) {
    FakeOtaFileSystem fileSystem;
    const UploadStatus current = makeIdle(4, ConfigSlot::secondary, 12, 14);
    seedRecord(fileSystem, primaryPath, current);
    UploadJournal journal(fileSystem);
    ASSERT_EQ(journal.recover().mode, JournalMode::recovered);
    fileSystem.setCloseFailure(backupPath);

    EXPECT_FALSE(journal.mirror());
    EXPECT_FALSE(journal.writable());
    const auto primaryAfterMirror = fileSystem.contents(primaryPath);
    const auto backupAfterMirror = fileSystem.contents(backupPath);
    const size_t mutationsAfterMirror = mutationCount(fileSystem);

    EXPECT_FALSE(journal.store(makeIdle(0, ConfigSlot::secondary, 12, 14)));
    EXPECT_EQ(mutationCount(fileSystem), mutationsAfterMirror);
    EXPECT_EQ(fileSystem.contents(primaryPath), primaryAfterMirror);
    EXPECT_EQ(fileSystem.contents(backupPath), backupAfterMirror);
}

TEST(UploadJournalTest, UnreadableNewerCopyCannotBeOverwritten) {
    FakeOtaFileSystem fileSystem;
    const UploadStatus newer = makeIdle(8, ConfigSlot::secondary, 12, 14);
    const UploadStatus older = makeIdle(7, ConfigSlot::primary, 10, 13);
    seedRecord(fileSystem, primaryPath, newer);
    seedRecord(fileSystem, backupPath, older);
    const auto originalPrimary = fileSystem.contents(primaryPath);
    const auto originalBackup = fileSystem.contents(backupPath);
    fileSystem.setReadErrorAfter(primaryPath, 0);

    UploadJournal journal(fileSystem);
    const JournalRecovery recovered = journal.recover();
    ASSERT_EQ(recovered.mode, JournalMode::recovered);
    ASSERT_TRUE(recovered.state.has_value());
    EXPECT_EQ(recovered.state->sequence, older.sequence);
    EXPECT_FALSE(journal.writable());
    EXPECT_FALSE(journal.mirror());
    EXPECT_EQ(fileSystem.contents(primaryPath), originalPrimary);
    EXPECT_EQ(fileSystem.contents(backupPath), originalBackup);
}

TEST(UploadJournalTest, PreviousMemoryDoesNotAuthorizeConflictingRepair) {
    FakeOtaFileSystem fileSystem;
    const UploadStatus previous = makeIdle(8, ConfigSlot::secondary, 12, 14);
    const UploadStatus conflict = makeIdle(8, ConfigSlot::primary, 10, 14);
    seedRecord(fileSystem, primaryPath, previous);

    UploadJournal journal(fileSystem);
    ASSERT_EQ(journal.recover().mode, JournalMode::recovered);
    seedRecord(fileSystem, primaryPath, previous);
    seedRecord(fileSystem, backupPath, conflict);
    const auto originalPrimary = fileSystem.contents(primaryPath);
    const auto originalBackup = fileSystem.contents(backupPath);

    EXPECT_EQ(journal.recover().mode, JournalMode::corrupt);
    EXPECT_FALSE(journal.mirror());
    EXPECT_EQ(fileSystem.contents(primaryPath), originalPrimary);
    EXPECT_EQ(fileSystem.contents(backupPath), originalBackup);
}

TEST(UploadJournalTest, MirrorFaultSweepPreservesPreexistingFloor) {
    const UploadStatus confirmed = makeIdle(11, ConfigSlot::secondary, 12, 14);

    FakeOtaFileSystem baseline;
    seedRecord(baseline, primaryPath, confirmed);
    UploadJournal baselineJournal(baseline);
    ASSERT_EQ(baselineJournal.recover().mode, JournalMode::recovered);
    const size_t mirrorStart = baseline.operations();
    ASSERT_TRUE(baselineJournal.mirror());
    const size_t mirrorOperations = baseline.operations() - mirrorStart;
    ASSERT_GT(mirrorOperations, 0u);

    for (size_t offset = 0; offset < mirrorOperations; ++offset) {
        SCOPED_TRACE(offset);
        FakeOtaFileSystem fileSystem;
        seedRecord(fileSystem, primaryPath, confirmed);
        UploadJournal journal(fileSystem);
        ASSERT_EQ(journal.recover().mode, JournalMode::recovered);
        fileSystem.failBefore(fileSystem.operations() + offset);

        bool interrupted = false;
        try {
            journal.mirror();
        } catch (const FakeOtaFileSystem::PowerCut&) {
            interrupted = true;
        }
        ASSERT_TRUE(interrupted);

        fileSystem.reboot();
        const auto validPrimary = decodedContents(fileSystem, primaryPath);
        const auto validBackup = decodedContents(fileSystem, backupPath);
        ASSERT_TRUE(validPrimary.has_value() || validBackup.has_value());
        const UploadStatus recovered =
            expectRecovered(fileSystem, ConfigSlot::secondary, 14);
        expectBoundaryPreserved(recovered, std::optional<uint32_t>{14});
    }
}

TEST(UploadJournalTest, StoreFaultMatrixPreservesPreexistingFloor) {
    const UploadStatus desired = makeIdle(0, ConfigSlot::secondary, 12, 14);
    const std::vector<JournalCopies> scenarios = {
        {makeIdle(7, ConfigSlot::secondary, 12, 14), std::nullopt},
        {std::nullopt, makeIdle(7, ConfigSlot::secondary, 12, 14)},
        {makeIdle(7, ConfigSlot::primary, 10, 13),
         makeIdle(8, ConfigSlot::secondary, 12, 14)},
        {makeIdle(8, ConfigSlot::secondary, 12, 14),
         makeIdle(7, ConfigSlot::primary, 10, 13)}};

    for (size_t scenarioIndex = 0; scenarioIndex < scenarios.size();
         ++scenarioIndex) {
        SCOPED_TRACE(scenarioIndex);
        FakeOtaFileSystem baseline;
        seedCopies(baseline, scenarios[scenarioIndex]);
        UploadJournal baselineJournal(baseline);
        const JournalRecovery baselineRecovery = baselineJournal.recover();
        ASSERT_EQ(baselineRecovery.mode, JournalMode::recovered);
        ASSERT_TRUE(baselineRecovery.state.has_value());
        const auto preexistingBoundary =
            baselineRecovery.state->minimumAcceptedConfigVersion;
        const auto originalPrimary = baseline.contents(primaryPath);
        const auto originalBackup = baseline.contents(backupPath);
        const size_t storeStart = baseline.operations();

        if (!baselineJournal.store(
                preexistingBoundary.has_value()
                    ? desired
                    : makeIdle(
                          0, ConfigSlot::secondary, UINT32_MAX,
                          std::nullopt))) {
            EXPECT_EQ(baseline.operations(), storeStart);
            EXPECT_EQ(baseline.contents(primaryPath), originalPrimary);
            EXPECT_EQ(baseline.contents(backupPath), originalBackup);
            continue;
        }
        const size_t storeOperations = baseline.operations() - storeStart;
        ASSERT_GT(storeOperations, 0u);

        for (size_t offset = 0; offset < storeOperations; ++offset) {
            SCOPED_TRACE(offset);
            FakeOtaFileSystem fileSystem;
            seedCopies(fileSystem, scenarios[scenarioIndex]);
            UploadJournal journal(fileSystem);
            const JournalRecovery beforeStore = journal.recover();
            ASSERT_EQ(beforeStore.mode, JournalMode::recovered);
            ASSERT_TRUE(beforeStore.state.has_value());
            EXPECT_EQ(
                beforeStore.state->minimumAcceptedConfigVersion,
                preexistingBoundary);
            fileSystem.failBefore(fileSystem.operations() + offset);

            bool interrupted = false;
            try {
                journal.store(
                    preexistingBoundary.has_value()
                        ? desired
                        : makeIdle(
                              0, ConfigSlot::secondary, UINT32_MAX,
                              std::nullopt));
            } catch (const FakeOtaFileSystem::PowerCut&) {
                interrupted = true;
            }
            ASSERT_TRUE(interrupted);

            fileSystem.reboot();
            const auto validPrimary = decodedContents(fileSystem, primaryPath);
            const auto validBackup = decodedContents(fileSystem, backupPath);
            ASSERT_TRUE(validPrimary.has_value() || validBackup.has_value());
            const JournalRecovery afterCut =
                UploadJournal(fileSystem).recover();
            ASSERT_EQ(afterCut.mode, JournalMode::recovered);
            ASSERT_TRUE(afterCut.state.has_value());
            EXPECT_EQ(afterCut.state->activeSlot, ConfigSlot::secondary);
            expectBoundaryPreserved(*afterCut.state, preexistingBoundary);
        }
    }
}

TEST(FakeOtaFileSystemTest, ShortWritePersistsPrefixAcrossReboot) {
    FakeOtaFileSystem fileSystem;
    fileSystem.seed(primaryPath, "old contents");
    auto file = fileSystem.open(primaryPath, OtaFileMode::truncate);
    ASSERT_NE(file, nullptr);
    const std::string bytes = "complete record";
    fileSystem.shortWriteAfter(7);

    EXPECT_EQ(
        file->write(
            SignatureBytes{
                reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()}),
        7u);
    EXPECT_TRUE(file->failed());
    EXPECT_EQ(
        fileSystem.contents(primaryPath),
        std::optional<std::string>{"complet"});

    fileSystem.reboot();
    EXPECT_EQ(
        fileSystem.contents(primaryPath),
        std::optional<std::string>{"complet"});
}
