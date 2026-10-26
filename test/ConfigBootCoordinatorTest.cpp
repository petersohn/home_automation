#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "FakeOtaFileSystem.hpp"
#include "common/ConfigBootCoordinator.hpp"
#include "common/ConfigParser.hpp"
#include "common/OtaTypes.hpp"
#include "common/UploadJournal.hpp"
#include "common/UploadStatus.hpp"

namespace {

constexpr const char* primaryPath = "/device_config.json";
constexpr const char* secondaryPath = "/device_config_2.json";
constexpr const char* statusPath = "/upload_status.json";
constexpr const char* backupPath = "/upload_status_backup.json";
constexpr const char* downloadPath = "/device_config.download.json";
constexpr ConfigSlot secondarySlot = ConfigSlot::secondary;
const std::string trialHash(64, 'a');

std::string makeConfig(
    std::optional<uint32_t> version, std::string extraFields = "") {
    std::string result = R"({"otaEnabled":true)";
    if (version.has_value()) {
        result += R"(,"configVersion":)" + std::to_string(*version);
    }
    result += R"(,"servicePrefix":"home/device")";
    result += extraFields;
    result += "}";
    return result;
}

UploadStatus makeIdle(
    uint32_t sequence, ConfigSlot activeSlot, uint32_t activeVersion,
    std::optional<uint32_t> minimum) {
    UploadStatus state;
    state.sequence = sequence;
    state.activeSlot = activeSlot;
    state.activeVersion = activeVersion;
    state.minimumAcceptedConfigVersion = minimum;
    return state;
}

UploadStatus makeTrial(
    uint32_t sequence, UploadPhase phase, uint32_t activeVersion,
    uint32_t trialVersion, std::optional<uint32_t> minimum,
    uint32_t trialBytes = 128) {
    UploadStatus state =
        makeIdle(sequence, ConfigSlot::primary, activeVersion, minimum);
    state.phase = phase;
    state.trialSlot = ConfigSlot::secondary;
    state.trialVersion = trialVersion;
    state.trialHash = trialHash;
    state.trialBytes = trialBytes;

    OtaResult staged;
    staged.status = "STAGED";
    staged.hash = trialHash;
    staged.version = trialVersion;
    state.lastOutcome = staged;
    return state;
}

void seedRecord(
    FakeOtaFileSystem& fileSystem, const char* path,
    const UploadStatus& state) {
    fileSystem.seed(path, encodeUploadStatus(state));
}

void seedCopies(FakeOtaFileSystem& fileSystem, const UploadStatus& state) {
    seedRecord(fileSystem, statusPath, state);
    seedRecord(fileSystem, backupPath, state);
}

class RemoveFailingOtaFileSystem final : public OtaFileSystem {
public:
    explicit RemoveFailingOtaFileSystem(FakeOtaFileSystem& fileSystem)
        : fileSystem(fileSystem) {}

    OtaFilePresence inspect(const char* path) override {
        return this->fileSystem.inspect(path);
    }

    std::unique_ptr<OtaFile> open(const char* path, OtaFileMode mode) override {
        return this->fileSystem.open(path, mode);
    }

    bool remove(const char* path) override {
        if (std::string(path) == downloadPath) {
            ++this->failedRemovals;
            return false;
        }
        return this->fileSystem.remove(path);
    }

    size_t failedRemovals = 0;

private:
    FakeOtaFileSystem& fileSystem;
};

void seedConfirmedBoot(
    FakeOtaFileSystem& fileSystem, uint32_t version = 11,
    std::optional<uint32_t> minimum = std::optional<uint32_t>{12}) {
    fileSystem.seed(primaryPath, makeConfig(version));
    fileSystem.seed(
        secondaryPath,
        makeConfig(version == UINT32_MAX ? version : version + 1));
    seedCopies(fileSystem, makeIdle(8, ConfigSlot::primary, version, minimum));
}

std::optional<UploadStatus> decodedContents(
    const FakeOtaFileSystem& fileSystem, const char* path) {
    const auto contents = fileSystem.contents(path);
    if (!contents.has_value()) {
        return std::nullopt;
    }
    UploadStatus state;
    if (!decodeUploadStatus(*contents, state)) {
        return std::nullopt;
    }
    return state;
}

void expectFloorAtLeast(const UploadStatus& state, uint32_t minimum) {
    ASSERT_TRUE(state.minimumAcceptedConfigVersion.has_value());
    EXPECT_GE(*state.minimumAcceptedConfigVersion, minimum);
}

void expectRollback(
    const UploadStatus& state, uint32_t activeVersion,
    std::optional<uint32_t> minimum) {
    EXPECT_EQ(state.phase, UploadPhase::idle);
    EXPECT_EQ(state.activeSlot, ConfigSlot::primary);
    EXPECT_EQ(state.activeVersion, activeVersion);
    EXPECT_EQ(state.minimumAcceptedConfigVersion, minimum);
    ASSERT_TRUE(state.lastOutcome.has_value());
    EXPECT_EQ(state.lastOutcome->status, "ROLLED_BACK");
}

void expectConfirmedTrial(const UploadStatus& state, uint32_t version) {
    EXPECT_EQ(state.phase, UploadPhase::idle);
    EXPECT_EQ(state.activeSlot, ConfigSlot::secondary);
    EXPECT_EQ(state.activeVersion, version);
    ASSERT_TRUE(state.minimumAcceptedConfigVersion.has_value());
    EXPECT_GE(*state.minimumAcceptedConfigVersion, version + 1);
    ASSERT_TRUE(state.lastOutcome.has_value());
    EXPECT_EQ(state.lastOutcome->status, "CONFIRMED");
}

bool isStatusOperation(const std::string& operation) {
    return operation.find(statusPath) != std::string::npos ||
           operation.find(backupPath) != std::string::npos;
}

void expectSafeBootAfterInterruptedStage(FakeOtaFileSystem& fileSystem) {
    fileSystem.reboot();
    const auto primary = decodedContents(fileSystem, statusPath);
    const auto backup = decodedContents(fileSystem, backupPath);
    ASSERT_TRUE(primary.has_value() || backup.has_value());
    for (const std::optional<UploadStatus>& persisted : {primary, backup}) {
        if (persisted.has_value()) {
            expectFloorAtLeast(*persisted, 12);
            EXPECT_EQ(persisted->activeSlot, ConfigSlot::primary);
            EXPECT_EQ(persisted->activeVersion, 11u);
        }
    }
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);
    const ConfigBootSelection selection = boot.selectForBoot();
    ASSERT_TRUE(journal.state().has_value());
    EXPECT_EQ(journal.state()->activeSlot, ConfigSlot::primary);
    EXPECT_EQ(journal.state()->activeVersion, 11u);
    expectFloorAtLeast(*journal.state(), 12);
    if (selection.trial) {
        EXPECT_EQ(selection.slot, ConfigSlot::secondary);
        EXPECT_EQ(journal.state()->phase, UploadPhase::started);
        expectFloorAtLeast(*journal.state(), 13);
    } else {
        EXPECT_EQ(selection.slot, ConfigSlot::primary);
    }
}

void expectSafeBootAfterInterruptedConfirmation(FakeOtaFileSystem& fileSystem) {
    fileSystem.reboot();
    const auto primary = decodedContents(fileSystem, statusPath);
    const auto backup = decodedContents(fileSystem, backupPath);
    ASSERT_TRUE(primary.has_value() || backup.has_value());
    for (const std::optional<UploadStatus>& persisted : {primary, backup}) {
        if (persisted.has_value()) {
            expectFloorAtLeast(*persisted, 13);
        }
    }
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);
    const ConfigBootSelection selection = boot.selectForBoot();
    ASSERT_TRUE(journal.state().has_value());
    EXPECT_FALSE(selection.trial);
    expectFloorAtLeast(*journal.state(), 13);
    if (selection.slot == ConfigSlot::secondary) {
        expectConfirmedTrial(*journal.state(), 12);
    } else {
        EXPECT_EQ(selection.slot, ConfigSlot::primary);
        expectRollback(*journal.state(), 11, std::optional<uint32_t>{13});
    }
}

}  // namespace

TEST(ConfigBootCoordinatorTest, FreshSerialConfigStartsAtOne) {
    FakeOtaFileSystem fileSystem;
    fileSystem.seed(primaryPath, R"({"servicePrefix":"home/device"})");
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);

    const ConfigBootSelection selection = boot.selectForBoot();

    EXPECT_EQ(selection.slot, ConfigSlot::primary);
    EXPECT_FALSE(selection.trial);
    EXPECT_FALSE(selection.otaAllowed);
    ASSERT_TRUE(journal.state().has_value());
    EXPECT_EQ(journal.state()->sequence, 1u);
    EXPECT_EQ(journal.state()->activeVersion, 0u);
    EXPECT_EQ(
        journal.state()->minimumAcceptedConfigVersion,
        std::optional<uint32_t>{1});
    EXPECT_EQ(boot.snapshot().configVersion, 0u);
}

TEST(ConfigBootCoordinatorTest, FreshVersionedConfigInitializesAcceptedFloor) {
    FakeOtaFileSystem fileSystem;
    fileSystem.seed(primaryPath, makeConfig(11));
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);

    const ConfigBootSelection selection = boot.selectForBoot();

    EXPECT_EQ(selection.slot, ConfigSlot::primary);
    EXPECT_FALSE(selection.trial);
    EXPECT_TRUE(selection.otaAllowed);
    ASSERT_TRUE(journal.state().has_value());
    EXPECT_EQ(journal.state()->sequence, 1u);
    EXPECT_EQ(journal.state()->activeVersion, 11u);
    EXPECT_EQ(
        journal.state()->minimumAcceptedConfigVersion,
        std::optional<uint32_t>{12});
}

TEST(ConfigBootCoordinatorTest, CorruptStatusBootsPrimaryWithoutUpdates) {
    for (bool omitPrimary : {false, true}) {
        SCOPED_TRACE(omitPrimary);
        FakeOtaFileSystem fileSystem;
        fileSystem.seed(primaryPath, makeConfig(19));
        if (!omitPrimary) {
            fileSystem.seed(statusPath, "not a valid status record");
        }
        fileSystem.seed(backupPath, "also not a valid status record");
        const auto oldPrimary = fileSystem.contents(statusPath);
        const auto oldBackup = fileSystem.contents(backupPath);
        UploadJournal journal(fileSystem);
        ConfigBootCoordinator boot(fileSystem, journal);

        const ConfigBootSelection selection = boot.selectForBoot();

        EXPECT_EQ(selection.slot, ConfigSlot::primary);
        EXPECT_FALSE(selection.trial);
        EXPECT_FALSE(selection.otaAllowed);
        EXPECT_FALSE(journal.state().has_value());
        EXPECT_EQ(fileSystem.contents(statusPath), oldPrimary);
        EXPECT_EQ(fileSystem.contents(backupPath), oldBackup);
    }
}

TEST(ConfigBootCoordinatorTest, MetadataIoErrorBootsConfirmedSlotClosed) {
    FakeOtaFileSystem fileSystem;
    fileSystem.seed(primaryPath, makeConfig(11));
    fileSystem.setInspectError(statusPath);
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);

    const ConfigBootSelection selection = boot.selectForBoot();

    EXPECT_EQ(selection.slot, ConfigSlot::primary);
    EXPECT_FALSE(selection.trial);
    EXPECT_FALSE(selection.otaAllowed);
    EXPECT_FALSE(journal.state().has_value());
}

TEST(
    ConfigBootCoordinatorTest,
    PrimaryCandidateNeverOverridesValidSecondaryRecord) {
    FakeOtaFileSystem fileSystem;
    fileSystem.seed(primaryPath, makeConfig(29));
    fileSystem.seed(secondaryPath, makeConfig(11));
    seedRecord(
        fileSystem, backupPath,
        makeIdle(4, ConfigSlot::secondary, 11, std::optional<uint32_t>{12}));
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);

    const ConfigBootSelection selection = boot.selectForBoot();

    EXPECT_EQ(selection.slot, ConfigSlot::secondary);
    EXPECT_FALSE(selection.trial);
    ASSERT_TRUE(journal.state().has_value());
    EXPECT_EQ(journal.state()->activeSlot, ConfigSlot::secondary);
    EXPECT_EQ(journal.state()->activeVersion, 11u);
}

TEST(ConfigBootCoordinatorTest, FreshRecoveryReadsMetadataBeforePrimaryConfig) {
    FakeOtaFileSystem fileSystem;
    fileSystem.seed(primaryPath, makeConfig(11));
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);

    const ConfigBootSelection selection = boot.selectForBoot();

    EXPECT_EQ(selection.slot, ConfigSlot::primary);
    const auto& operations = fileSystem.operationLog();
    const auto firstPrimaryRead = std::find(
        operations.begin(), operations.end(),
        std::string("open-read:") + primaryPath);
    ASSERT_NE(firstPrimaryRead, operations.end());
    const auto primaryStatusInspect = std::find(
        operations.begin(), operations.end(),
        std::string("inspect:") + statusPath);
    const auto backupStatusInspect = std::find(
        operations.begin(), operations.end(),
        std::string("inspect:") + backupPath);
    ASSERT_NE(primaryStatusInspect, operations.end());
    ASSERT_NE(backupStatusInspect, operations.end());
    EXPECT_LT(primaryStatusInspect, firstPrimaryRead);
    EXPECT_LT(backupStatusInspect, firstPrimaryRead);
}

TEST(ConfigBootCoordinatorTest, RemovesAbandonedDownloadAfterJournalRecovery) {
    FakeOtaFileSystem fileSystem;
    seedConfirmedBoot(fileSystem);
    fileSystem.seed(downloadPath, "partial signed envelope");
    const auto statusBefore = fileSystem.contents(statusPath);
    const auto backupBefore = fileSystem.contents(backupPath);
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);

    const ConfigBootSelection selection = boot.selectForBoot();

    EXPECT_EQ(selection.slot, ConfigSlot::primary);
    EXPECT_FALSE(selection.trial);
    EXPECT_TRUE(selection.otaAllowed);
    EXPECT_FALSE(fileSystem.contents(downloadPath).has_value());
    EXPECT_EQ(fileSystem.contents(statusPath), statusBefore);
    EXPECT_EQ(fileSystem.contents(backupPath), backupBefore);

    const auto& operations = fileSystem.operationLog();
    const auto statusRead = std::find(
        operations.begin(), operations.end(),
        std::string("open-read:") + statusPath);
    const auto backupRead = std::find(
        operations.begin(), operations.end(),
        std::string("open-read:") + backupPath);
    const auto downloadRemoval = std::find(
        operations.begin(), operations.end(),
        std::string("remove:") + downloadPath);
    ASSERT_NE(statusRead, operations.end());
    ASSERT_NE(backupRead, operations.end());
    ASSERT_NE(downloadRemoval, operations.end());
    EXPECT_LT(statusRead, downloadRemoval);
    EXPECT_LT(backupRead, downloadRemoval);
}

TEST(
    ConfigBootCoordinatorTest,
    FailedDownloadCleanupKeepsConfirmedBootAndJournal) {
    FakeOtaFileSystem fileSystem;
    seedConfirmedBoot(fileSystem);
    fileSystem.seed(downloadPath, "partial signed envelope");
    fileSystem.setInspectError(downloadPath);
    const auto statusBefore = fileSystem.contents(statusPath);
    const auto backupBefore = fileSystem.contents(backupPath);
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);

    const ConfigBootSelection selection = boot.selectForBoot();

    EXPECT_EQ(selection.slot, ConfigSlot::primary);
    EXPECT_FALSE(selection.trial);
    EXPECT_FALSE(selection.otaAllowed);
    EXPECT_TRUE(fileSystem.contents(downloadPath).has_value());
    ASSERT_TRUE(journal.state().has_value());
    EXPECT_EQ(journal.state()->activeSlot, ConfigSlot::primary);
    EXPECT_EQ(journal.state()->activeVersion, 11u);
    EXPECT_EQ(
        journal.state()->minimumAcceptedConfigVersion,
        std::optional<uint32_t>{12});
    EXPECT_FALSE(boot.snapshot().updatesAllowed);
    EXPECT_EQ(fileSystem.contents(statusPath), statusBefore);
    EXPECT_EQ(fileSystem.contents(backupPath), backupBefore);
    EXPECT_EQ(
        std::find(
            fileSystem.operationLog().begin(), fileSystem.operationLog().end(),
            std::string("open-truncate:") + statusPath),
        fileSystem.operationLog().end());
    EXPECT_EQ(
        std::find(
            fileSystem.operationLog().begin(), fileSystem.operationLog().end(),
            std::string("open-truncate:") + backupPath),
        fileSystem.operationLog().end());
}

TEST(
    ConfigBootCoordinatorTest,
    FreshCleanupRemoveFailureDoesNotInitializeJournal) {
    FakeOtaFileSystem fileSystem;
    fileSystem.seed(primaryPath, makeConfig(11));
    fileSystem.seed(downloadPath, "abandoned signed envelope");
    RemoveFailingOtaFileSystem failingFileSystem(fileSystem);
    UploadJournal journal(failingFileSystem);
    ConfigBootCoordinator boot(failingFileSystem, journal);

    const ConfigBootSelection selection = boot.selectForBoot();

    EXPECT_EQ(selection.slot, ConfigSlot::primary);
    EXPECT_FALSE(selection.trial);
    EXPECT_FALSE(selection.otaAllowed);
    EXPECT_EQ(failingFileSystem.failedRemovals, 1u);
    EXPECT_TRUE(fileSystem.contents(downloadPath).has_value());
    EXPECT_FALSE(fileSystem.contents(statusPath).has_value());
    EXPECT_FALSE(fileSystem.contents(backupPath).has_value());
    EXPECT_FALSE(journal.state().has_value());
    EXPECT_EQ(boot.snapshot().configVersion, 11u);
    EXPECT_EQ(
        std::find(
            fileSystem.operationLog().begin(), fileSystem.operationLog().end(),
            std::string("open-truncate:") + statusPath),
        fileSystem.operationLog().end());
    EXPECT_EQ(
        std::find(
            fileSystem.operationLog().begin(), fileSystem.operationLog().end(),
            std::string("open-truncate:") + backupPath),
        fileSystem.operationLog().end());
}

TEST(ConfigBootCoordinatorTest, PendingTrialMarksStartedBeforeInitialization) {
    FakeOtaFileSystem fileSystem;
    const std::string candidate = makeConfig(12);
    fileSystem.seed(primaryPath, makeConfig(11));
    fileSystem.seed(secondaryPath, candidate);
    seedRecord(
        fileSystem, backupPath,
        makeTrial(9, UploadPhase::pending, 11, 12, 13, candidate.size()));
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);

    const ConfigBootSelection selection = boot.selectForBoot();

    EXPECT_EQ(selection.slot, ConfigSlot::secondary);
    EXPECT_TRUE(selection.trial);
    ASSERT_TRUE(journal.state().has_value());
    EXPECT_EQ(journal.state()->phase, UploadPhase::started);
    EXPECT_EQ(journal.state()->trialSlot, ConfigSlot::secondary);
    EXPECT_EQ(journal.state()->trialVersion, 12u);
    EXPECT_EQ(
        journal.state()->minimumAcceptedConfigVersion,
        std::optional<uint32_t>{13});

    const auto persistedPrimary = decodedContents(fileSystem, statusPath);
    const auto persistedBackup = decodedContents(fileSystem, backupPath);
    ASSERT_TRUE(persistedPrimary.has_value() || persistedBackup.has_value());
    const bool primaryStarted = persistedPrimary.has_value() &&
                                persistedPrimary->phase == UploadPhase::started;
    const bool backupStarted = persistedBackup.has_value() &&
                               persistedBackup->phase == UploadPhase::started;
    EXPECT_TRUE(primaryStarted || backupStarted);

    const auto& operations = fileSystem.operationLog();
    const auto candidateOpen = std::find(
        operations.begin(), operations.end(),
        std::string("open-read:") + secondaryPath);
    ASSERT_NE(candidateOpen, operations.end());
    const auto startedWrite = std::find_if(
        operations.begin(), candidateOpen, [](const std::string& operation) {
        return operation.find("open-truncate:") != std::string::npos &&
               isStatusOperation(operation);
    });
    EXPECT_NE(startedWrite, candidateOpen);
}

TEST(
    ConfigBootCoordinatorTest,
    PendingCandidateMustBeRootConfigWithMatchingPolicy) {
    const std::vector<std::string> invalidCandidates = {
        makeConfig(13),
        R"({"otaEnabled":false,"configVersion":12,"servicePrefix":"home/device"})",
        R"({"type":"device-config","target":"AA:BB:CC:DD:EE:FF","config":{"otaEnabled":true,"configVersion":12,"servicePrefix":"home/device"}})"};

    for (std::string candidate : invalidCandidates) {
        SCOPED_TRACE(candidate);
        FakeOtaFileSystem fileSystem;
        fileSystem.seed(primaryPath, makeConfig(11));
        fileSystem.seed(secondaryPath, candidate);
        seedCopies(
            fileSystem,
            makeTrial(9, UploadPhase::pending, 11, 12, 13, candidate.size()));
        UploadJournal journal(fileSystem);
        ConfigBootCoordinator boot(fileSystem, journal);

        const ConfigBootSelection selection = boot.selectForBoot();

        EXPECT_EQ(selection.slot, ConfigSlot::primary);
        EXPECT_FALSE(selection.trial);
        ASSERT_TRUE(journal.state().has_value());
        expectRollback(*journal.state(), 11, std::optional<uint32_t>{13});
    }
}

TEST(ConfigBootCoordinatorTest, InvalidInterfaceEntriesDoNotRejectCandidate) {
    const std::string candidate = makeConfig(
        12, R"(,"interfaces":[{"type":"input","name":"missing-pin"}])");
    FakeOtaFileSystem fileSystem;
    fileSystem.seed(primaryPath, makeConfig(11));
    fileSystem.seed(secondaryPath, candidate);
    seedCopies(
        fileSystem,
        makeTrial(9, UploadPhase::pending, 11, 12, 13, candidate.size()));
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);

    const ConfigBootSelection selection = boot.selectForBoot();

    EXPECT_EQ(selection.slot, ConfigSlot::secondary);
    EXPECT_TRUE(selection.trial);

    ArduinoJson::DynamicJsonBuffer buffer{1024};
    auto& root = buffer.parseObject(candidate);
    ASSERT_NE(root, ArduinoJson::JsonObject::invalid());
    std::ostringstream debug;
    ConfigParser parser(debug);
    const auto parsed = parser.parseDeviceConfig(root);
    EXPECT_TRUE(parsed.interfaces.empty());
}

TEST(ConfigBootCoordinatorTest, InterruptedTrialRevertsRegardlessResetReason) {
    FakeOtaFileSystem fileSystem;
    fileSystem.seed(primaryPath, makeConfig(11));
    fileSystem.seed(secondaryPath, makeConfig(12));
    seedCopies(fileSystem, makeTrial(10, UploadPhase::started, 11, 12, 13));
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);

    const ConfigBootSelection selection = boot.selectForBoot();

    EXPECT_EQ(selection.slot, ConfigSlot::primary);
    EXPECT_FALSE(selection.trial);
    ASSERT_TRUE(journal.state().has_value());
    expectRollback(*journal.state(), 11, std::optional<uint32_t>{13});
    EXPECT_EQ(boot.snapshot().configVersion, 11u);
    EXPECT_EQ(
        boot.snapshot().minimumAcceptedConfigVersion,
        std::optional<uint32_t>{13});
}

TEST(ConfigBootCoordinatorTest, CollisionRollbackRetainsMinimum) {
    FakeOtaFileSystem fileSystem;
    const std::string candidate = makeConfig(12);
    fileSystem.seed(primaryPath, makeConfig(11));
    fileSystem.seed(secondaryPath, candidate);
    seedCopies(
        fileSystem,
        makeTrial(10, UploadPhase::pending, 11, 12, 13, candidate.size()));
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);
    ASSERT_TRUE(boot.selectForBoot().trial);

    EXPECT_TRUE(boot.rollback("candidate-collision"));
    ASSERT_TRUE(journal.state().has_value());
    expectRollback(*journal.state(), 11, std::optional<uint32_t>{13});
}

TEST(ConfigBootCoordinatorTest, MaximumVersionRollbackKeepsExhaustedFloor) {
    FakeOtaFileSystem fileSystem;
    fileSystem.seed(primaryPath, makeConfig(UINT32_MAX - 1));
    fileSystem.seed(secondaryPath, makeConfig(UINT32_MAX));
    seedCopies(
        fileSystem, makeTrial(
                        10, UploadPhase::started, UINT32_MAX - 1, UINT32_MAX,
                        std::nullopt));
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);

    const ConfigBootSelection selection = boot.selectForBoot();

    EXPECT_EQ(selection.slot, ConfigSlot::primary);
    EXPECT_FALSE(selection.trial);
    ASSERT_TRUE(journal.state().has_value());
    expectRollback(*journal.state(), UINT32_MAX - 1, std::nullopt);
    EXPECT_FALSE(boot.snapshot().minimumAcceptedConfigVersion.has_value());
}

TEST(
    ConfigBootCoordinatorTest,
    RollbackPersistenceFailureClosesOtaAfterSequenceExhaustion) {
    FakeOtaFileSystem fileSystem;
    const std::string candidate = makeConfig(13);
    fileSystem.seed(primaryPath, makeConfig(11));
    fileSystem.seed(secondaryPath, candidate);
    seedCopies(
        fileSystem, makeTrial(
                        std::numeric_limits<uint32_t>::max() - 1,
                        UploadPhase::pending, 11, 12, 13, candidate.size()));
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);

    const ConfigBootSelection selection = boot.selectForBoot();

    EXPECT_EQ(selection.slot, ConfigSlot::primary);
    EXPECT_FALSE(selection.trial);
    EXPECT_FALSE(selection.otaAllowed);
    ASSERT_TRUE(journal.state().has_value());
    EXPECT_EQ(journal.state()->sequence, std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(journal.state()->phase, UploadPhase::started);
    EXPECT_TRUE(journal.writable());
    EXPECT_FALSE(boot.snapshot().updatesAllowed);
}

TEST(ConfigBootCoordinatorTest, TrialStatusReportsRunningCandidateVersion) {
    FakeOtaFileSystem fileSystem;
    const std::string candidate = makeConfig(12);
    fileSystem.seed(primaryPath, makeConfig(11));
    fileSystem.seed(secondaryPath, candidate);
    seedCopies(
        fileSystem,
        makeTrial(9, UploadPhase::pending, 11, 12, 13, candidate.size()));
    UploadJournal journal(fileSystem);
    ASSERT_EQ(journal.recover().mode, JournalMode::recovered);
    ConfigBootCoordinator boot(fileSystem, journal);

    EXPECT_EQ(boot.snapshot().configVersion, 11u);
    EXPECT_EQ(boot.snapshot().phase, UploadPhase::pending);

    const ConfigBootSelection selection = boot.selectForBoot();

    EXPECT_TRUE(selection.trial);
    EXPECT_EQ(boot.snapshot().configVersion, 12u);
    EXPECT_EQ(boot.snapshot().phase, UploadPhase::started);
    EXPECT_EQ(
        boot.snapshot().minimumAcceptedConfigVersion,
        std::optional<uint32_t>{13});

    EXPECT_TRUE(boot.rollback("reset"));
    EXPECT_EQ(boot.snapshot().configVersion, 11u);
    EXPECT_EQ(
        boot.snapshot().minimumAcceptedConfigVersion,
        std::optional<uint32_t>{13});
}

TEST(ConfigBootCoordinatorTest, ConfirmsOnlyExplicitly) {
    FakeOtaFileSystem fileSystem;
    const std::string candidate = makeConfig(12);
    fileSystem.seed(primaryPath, makeConfig(11));
    fileSystem.seed(secondaryPath, candidate);
    seedCopies(
        fileSystem,
        makeTrial(9, UploadPhase::pending, 11, 12, 13, candidate.size()));
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);

    const ConfigBootSelection selection = boot.selectForBoot();

    ASSERT_TRUE(selection.trial);
    ASSERT_TRUE(boot.trialRunning());
    ASSERT_TRUE(journal.state().has_value());
    EXPECT_EQ(journal.state()->phase, UploadPhase::started);
    EXPECT_EQ(journal.state()->activeSlot, ConfigSlot::primary);
    EXPECT_TRUE(boot.confirm());
    ASSERT_TRUE(journal.state().has_value());
    expectConfirmedTrial(*journal.state(), 12);
    EXPECT_FALSE(boot.trialRunning());
    EXPECT_EQ(boot.snapshot().configVersion, 12u);
    EXPECT_EQ(boot.snapshot().phase, UploadPhase::idle);
    ASSERT_TRUE(boot.snapshot().lastOutcome.has_value());
    EXPECT_EQ(boot.snapshot().lastOutcome->status, "CONFIRMED");
}

TEST(ConfigBootCoordinatorTest, RejectsStageWhileTrialPending) {
    const std::string candidate = makeConfig(12);
    for (UploadPhase phase : {UploadPhase::pending, UploadPhase::started}) {
        SCOPED_TRACE(static_cast<int>(phase));
        FakeOtaFileSystem fileSystem;
        fileSystem.seed(primaryPath, makeConfig(11));
        fileSystem.seed(secondaryPath, candidate);
        seedCopies(
            fileSystem, makeTrial(9, phase, 11, 12, 13, candidate.size()));
        UploadJournal journal(fileSystem);
        ASSERT_EQ(journal.recover().mode, JournalMode::recovered);
        ConfigBootCoordinator boot(fileSystem, journal);
        const auto primaryBefore = fileSystem.contents(statusPath);
        const auto backupBefore = fileSystem.contents(backupPath);

        EXPECT_FALSE(boot.stage(
            secondarySlot, 13, std::string(64, 'b'), candidate.size()));
        EXPECT_EQ(fileSystem.contents(statusPath), primaryBefore);
        EXPECT_EQ(fileSystem.contents(backupPath), backupBefore);
    }
}

TEST(ConfigBootCoordinatorTest, FailedStageOrStartCannotRunCandidate) {
    {
        FakeOtaFileSystem fileSystem;
        seedConfirmedBoot(fileSystem);
        const size_t bytes = fileSystem.contents(secondaryPath)->size();
        UploadJournal journal(fileSystem);
        ConfigBootCoordinator boot(fileSystem, journal);
        ASSERT_EQ(boot.selectForBoot().slot, ConfigSlot::primary);
        fileSystem.shortWriteAfter(0);

        EXPECT_FALSE(boot.stage(secondarySlot, 12, trialHash, bytes));

        fileSystem.reboot();
        UploadJournal rebootedJournal(fileSystem);
        ConfigBootCoordinator rebooted(fileSystem, rebootedJournal);
        const ConfigBootSelection selection = rebooted.selectForBoot();
        EXPECT_EQ(selection.slot, ConfigSlot::primary);
        EXPECT_FALSE(selection.trial);
    }

    {
        FakeOtaFileSystem fileSystem;
        const std::string candidate = makeConfig(12);
        fileSystem.seed(primaryPath, makeConfig(11));
        fileSystem.seed(secondaryPath, candidate);
        seedCopies(
            fileSystem,
            makeTrial(9, UploadPhase::pending, 11, 12, 13, candidate.size()));
        UploadJournal journal(fileSystem);
        ConfigBootCoordinator boot(fileSystem, journal);
        fileSystem.shortWriteAfter(0);

        const ConfigBootSelection selection = boot.selectForBoot();

        EXPECT_EQ(selection.slot, ConfigSlot::primary);
        EXPECT_FALSE(selection.trial);
        EXPECT_FALSE(selection.otaAllowed);
    }
}

TEST(
    ConfigBootCoordinatorTest,
    StageFaultSweepRecoversOnlyPersistedTransitions) {
    FakeOtaFileSystem baseline;
    seedConfirmedBoot(baseline);
    UploadJournal baselineJournal(baseline);
    ConfigBootCoordinator baselineBoot(baseline, baselineJournal);
    ASSERT_EQ(baselineBoot.selectForBoot().slot, ConfigSlot::primary);
    const size_t stageStart = baseline.operations();
    const size_t candidateBytes = baseline.contents(secondaryPath)->size();
    ASSERT_TRUE(
        baselineBoot.stage(secondarySlot, 12, trialHash, candidateBytes));
    const size_t stageOperations = baseline.operations() - stageStart;
    ASSERT_GT(stageOperations, 0u);

    for (size_t offset = 0; offset < stageOperations; ++offset) {
        SCOPED_TRACE(offset);
        FakeOtaFileSystem fileSystem;
        seedConfirmedBoot(fileSystem);
        UploadJournal journal(fileSystem);
        ConfigBootCoordinator boot(fileSystem, journal);
        ASSERT_EQ(boot.selectForBoot().slot, ConfigSlot::primary);
        const size_t bytes = fileSystem.contents(secondaryPath)->size();
        fileSystem.failBefore(fileSystem.operations() + offset);

        bool interrupted = false;
        try {
            boot.stage(secondarySlot, 12, trialHash, bytes);
        } catch (const FakeOtaFileSystem::PowerCut&) {
            interrupted = true;
        }
        ASSERT_TRUE(interrupted);
        expectSafeBootAfterInterruptedStage(fileSystem);
    }
}

TEST(ConfigBootCoordinatorTest, StartFaultSweepNeverReturnsUnstartedCandidate) {
    FakeOtaFileSystem baseline;
    const std::string candidate = makeConfig(12);
    baseline.seed(primaryPath, makeConfig(11));
    baseline.seed(secondaryPath, candidate);
    seedCopies(
        baseline,
        makeTrial(9, UploadPhase::pending, 11, 12, 13, candidate.size()));
    UploadJournal baselineJournal(baseline);
    ConfigBootCoordinator baselineBoot(baseline, baselineJournal);
    const ConfigBootSelection baselineSelection = baselineBoot.selectForBoot();
    ASSERT_TRUE(baselineSelection.trial);
    const size_t startOperations = baseline.operations();

    for (size_t offset = 0; offset < startOperations; ++offset) {
        SCOPED_TRACE(offset);
        FakeOtaFileSystem fileSystem;
        fileSystem.seed(primaryPath, makeConfig(11));
        fileSystem.seed(secondaryPath, candidate);
        seedCopies(
            fileSystem,
            makeTrial(9, UploadPhase::pending, 11, 12, 13, candidate.size()));
        UploadJournal journal(fileSystem);
        ConfigBootCoordinator boot(fileSystem, journal);
        fileSystem.failBefore(offset);

        bool interrupted = false;
        try {
            (void)boot.selectForBoot();
        } catch (const FakeOtaFileSystem::PowerCut&) {
            interrupted = true;
        }
        ASSERT_TRUE(interrupted);

        fileSystem.reboot();
        const auto primary = decodedContents(fileSystem, statusPath);
        const auto backup = decodedContents(fileSystem, backupPath);
        ASSERT_TRUE(primary.has_value() || backup.has_value());
        for (const std::optional<UploadStatus>& persisted : {primary, backup}) {
            if (persisted.has_value()) {
                expectFloorAtLeast(*persisted, 13);
                EXPECT_EQ(persisted->activeSlot, ConfigSlot::primary);
                EXPECT_EQ(persisted->activeVersion, 11u);
            }
        }

        UploadJournal recoveredJournal(fileSystem);
        const JournalRecovery recoveredState = recoveredJournal.recover();
        ASSERT_EQ(recoveredState.mode, JournalMode::recovered);
        ASSERT_TRUE(recoveredState.state.has_value());
        const UploadPhase persistedPhase = recoveredState.state->phase;
        ASSERT_TRUE(
            persistedPhase == UploadPhase::pending ||
            persistedPhase == UploadPhase::started);

        ConfigBootCoordinator recoveredBoot(fileSystem, recoveredJournal);
        const ConfigBootSelection recovered = recoveredBoot.selectForBoot();
        ASSERT_TRUE(recoveredJournal.state().has_value());
        EXPECT_EQ(recoveredJournal.state()->activeSlot, ConfigSlot::primary);
        EXPECT_EQ(recoveredJournal.state()->activeVersion, 11u);
        if (persistedPhase == UploadPhase::started) {
            EXPECT_EQ(recovered.slot, ConfigSlot::primary);
            EXPECT_FALSE(recovered.trial);
            expectRollback(
                *recoveredJournal.state(), 11, std::optional<uint32_t>{13});
        } else if (recovered.trial) {
            EXPECT_EQ(recovered.slot, ConfigSlot::secondary);
            EXPECT_EQ(recoveredJournal.state()->phase, UploadPhase::started);
        } else {
            EXPECT_EQ(recovered.slot, ConfigSlot::primary);
        }
    }
}

TEST(
    ConfigBootCoordinatorTest,
    ConfirmFaultSweepPreservesConfirmedOrRollbackState) {
    FakeOtaFileSystem baseline;
    const std::string candidate = makeConfig(12);
    baseline.seed(primaryPath, makeConfig(11));
    baseline.seed(secondaryPath, candidate);
    seedCopies(
        baseline,
        makeTrial(9, UploadPhase::pending, 11, 12, 13, candidate.size()));
    UploadJournal baselineJournal(baseline);
    ConfigBootCoordinator baselineBoot(baseline, baselineJournal);
    ASSERT_TRUE(baselineBoot.selectForBoot().trial);
    const size_t confirmationStart = baseline.operations();
    ASSERT_TRUE(baselineBoot.confirm());
    const size_t confirmationOperations =
        baseline.operations() - confirmationStart;
    ASSERT_GT(confirmationOperations, 0u);

    for (size_t offset = 0; offset < confirmationOperations; ++offset) {
        SCOPED_TRACE(offset);
        FakeOtaFileSystem fileSystem;
        fileSystem.seed(primaryPath, makeConfig(11));
        fileSystem.seed(secondaryPath, candidate);
        seedCopies(
            fileSystem,
            makeTrial(9, UploadPhase::pending, 11, 12, 13, candidate.size()));
        UploadJournal journal(fileSystem);
        ConfigBootCoordinator boot(fileSystem, journal);
        ASSERT_TRUE(boot.selectForBoot().trial);
        fileSystem.failBefore(fileSystem.operations() + offset);

        bool interrupted = false;
        try {
            (void)boot.confirm();
        } catch (const FakeOtaFileSystem::PowerCut&) {
            interrupted = true;
        }
        ASSERT_TRUE(interrupted);
        expectSafeBootAfterInterruptedConfirmation(fileSystem);
    }
}

TEST(ConfigBootCoordinatorTest, RollbackFaultSweepRetainsConfirmedFloor) {
    FakeOtaFileSystem baseline;
    const std::string candidate = makeConfig(12);
    baseline.seed(primaryPath, makeConfig(11));
    baseline.seed(secondaryPath, candidate);
    seedCopies(
        baseline,
        makeTrial(10, UploadPhase::pending, 11, 12, 13, candidate.size()));
    UploadJournal baselineJournal(baseline);
    ConfigBootCoordinator baselineBoot(baseline, baselineJournal);
    ASSERT_TRUE(baselineBoot.selectForBoot().trial);
    const size_t rollbackStart = baseline.operations();
    ASSERT_TRUE(baselineBoot.rollback("reset"));
    const size_t rollbackOperations = baseline.operations() - rollbackStart;
    ASSERT_GT(rollbackOperations, 0u);

    for (size_t offset = 0; offset < rollbackOperations; ++offset) {
        SCOPED_TRACE(offset);
        FakeOtaFileSystem fileSystem;
        fileSystem.seed(primaryPath, makeConfig(11));
        fileSystem.seed(secondaryPath, candidate);
        seedCopies(
            fileSystem,
            makeTrial(10, UploadPhase::pending, 11, 12, 13, candidate.size()));
        UploadJournal journal(fileSystem);
        ConfigBootCoordinator boot(fileSystem, journal);
        ASSERT_TRUE(boot.selectForBoot().trial);
        fileSystem.failBefore(fileSystem.operations() + offset);

        bool interrupted = false;
        try {
            (void)boot.rollback("reset");
        } catch (const FakeOtaFileSystem::PowerCut&) {
            interrupted = true;
        }
        ASSERT_TRUE(interrupted);

        fileSystem.reboot();
        const auto primary = decodedContents(fileSystem, statusPath);
        const auto backup = decodedContents(fileSystem, backupPath);
        ASSERT_TRUE(primary.has_value() || backup.has_value());
        for (const std::optional<UploadStatus>& persisted : {primary, backup}) {
            if (persisted.has_value()) {
                expectFloorAtLeast(*persisted, 13);
                EXPECT_EQ(persisted->activeSlot, ConfigSlot::primary);
                EXPECT_EQ(persisted->activeVersion, 11u);
            }
        }

        UploadJournal recoveredJournal(fileSystem);
        ConfigBootCoordinator recoveredBoot(fileSystem, recoveredJournal);
        const ConfigBootSelection recovered = recoveredBoot.selectForBoot();
        EXPECT_EQ(recovered.slot, ConfigSlot::primary);
        EXPECT_FALSE(recovered.trial);
        ASSERT_TRUE(recoveredJournal.state().has_value());
        expectRollback(
            *recoveredJournal.state(), 11, std::optional<uint32_t>{13});
    }
}
