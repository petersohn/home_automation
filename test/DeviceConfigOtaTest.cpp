#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "FakeEspApi.hpp"
#include "FakeOtaFileSystem.hpp"
#include "common/ConfigBootCoordinator.hpp"
#include "common/DeviceConfigOta.hpp"
#include "common/MqttClient.hpp"
#include "common/OtaTypes.hpp"
#include "common/Sha256.hpp"
#include "common/SignatureVerifier.hpp"
#include "common/UploadJournal.hpp"
#include "common/UploadStatus.hpp"

namespace {

constexpr const char* stationMac = "AA:BB:CC:DD:EE:FF";
constexpr const char* primaryPath = "/device_config.json";
constexpr const char* secondaryPath = "/device_config_2.json";
constexpr const char* downloadPath = "/device_config.download.json";
constexpr const char* journalPath = "/upload_status.json";
constexpr const char* backupPath = "/upload_status_backup.json";
const std::string streamHash =
    "a26d5f676588615528ed051a4d33dab6695c59f331e3dbfe8be7ac5da808fe18";

std::string makeConfig(
    uint32_t version, const std::string& prefix = "home/device",
    bool otaEnabled = true, const std::string& extraFields = "") {
    return R"({"otaEnabled":)" + std::string(otaEnabled ? "true" : "false") +
           R"(,"configVersion":)" + std::to_string(version) +
           R"(,"servicePrefix":")" + prefix + R"(")" + extraFields + "}";
}

std::string makeEnvelope(
    const std::string& config, const std::string& target = stationMac,
    const std::string& type = "device-config") {
    return R"({"type":")" + type + R"(","target":")" + target +
           R"(","config":)" + config + "}";
}

std::vector<uint8_t> bytesOf(const std::string& text) {
    std::vector<uint8_t> bytes;
    bytes.reserve(text.size());
    for (const unsigned char value : text) {
        bytes.push_back(value);
    }
    return bytes;
}

std::string sha256Hex(const std::string& text) {
    static constexpr char hexDigits[] = "0123456789abcdef";
    Sha256 hash;
    for (const unsigned char value : text) {
        hash.update(value);
    }
    const std::array<uint8_t, 32> digest = hash.finish();
    std::string result;
    result.reserve(64);
    for (const uint8_t value : digest) {
        result += hexDigits[value >> 4];
        result += hexDigits[value & 0x0F];
    }
    return result;
}

std::vector<uint8_t> hashBytes(const std::string& hex) {
    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);
    const auto digit = [](char value) -> uint8_t {
        if (value >= '0' && value <= '9') {
            return static_cast<uint8_t>(value - '0');
        }
        return static_cast<uint8_t>(value - 'a' + 10);
    };
    for (std::size_t index = 0; index + 1 < hex.size(); index += 2) {
        result.push_back(
            static_cast<uint8_t>(
                (digit(hex[index]) << 4) | digit(hex[index + 1])));
    }
    return result;
}

std::vector<uint8_t> copyBytes(SignatureBytes bytes) {
    std::vector<uint8_t> copied;
    copied.reserve(bytes.size);
    for (std::size_t index = 0; index < bytes.size; ++index) {
        copied.push_back(bytes.data[index]);
    }
    return copied;
}

std::string base64Encode(const std::vector<uint8_t>& bytes) {
    static const char* const alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    encoded.reserve(((bytes.size() + 2) / 3) * 4);
    std::size_t index = 0;
    while (index + 3 <= bytes.size()) {
        const uint32_t value = (static_cast<uint32_t>(bytes[index]) << 16) |
                               (static_cast<uint32_t>(bytes[index + 1]) << 8) |
                               static_cast<uint32_t>(bytes[index + 2]);
        encoded += alphabet[(value >> 18) & 0x3F];
        encoded += alphabet[(value >> 12) & 0x3F];
        encoded += alphabet[(value >> 6) & 0x3F];
        encoded += alphabet[value & 0x3F];
        index += 3;
    }
    const std::size_t remaining = bytes.size() - index;
    if (remaining == 1) {
        const uint32_t value = static_cast<uint32_t>(bytes[index]) << 16;
        encoded += alphabet[(value >> 18) & 0x3F];
        encoded += alphabet[(value >> 12) & 0x3F];
        encoded += "==";
    } else if (remaining == 2) {
        const uint32_t value = (static_cast<uint32_t>(bytes[index]) << 16) |
                               (static_cast<uint32_t>(bytes[index + 1]) << 8);
        encoded += alphabet[(value >> 18) & 0x3F];
        encoded += alphabet[(value >> 12) & 0x3F];
        encoded += alphabet[(value >> 6) & 0x3F];
        encoded += '=';
    }
    return encoded;
}

std::string beginCommand(const std::string& hash, uint32_t size) {
    return R"({"type":"begin","hash":")" + hash + R"(","size":)" +
           std::to_string(size) + "}";
}

std::string chunkCommand(
    const std::string& hash, uint32_t offset,
    const std::vector<uint8_t>& bytes) {
    return R"({"type":"chunk","hash":")" + hash + R"(","offset":)" +
           std::to_string(offset) + R"(,"data":")" + base64Encode(bytes) +
           R"("})";
}

std::string finishCommand(const std::string& hash) {
    return R"({"type":"finish","hash":")" + hash + R"(","signature":")" +
           base64Encode(std::vector<uint8_t>(64, 0xA5)) + R"("})";
}

class FakeVerifier final : public SignatureVerifier {
public:
    SignatureError beginError = SignatureError::none;
    SignatureError appendError = SignatureError::none;
    SignatureResult finishResult{SignatureStatus::valid, SignatureError::none};
    std::size_t beginCalls = 0;
    std::size_t appendCalls = 0;
    std::size_t finishCalls = 0;
    std::size_t abortCalls = 0;
    std::vector<std::vector<uint8_t>> receivedData;
    std::vector<std::vector<uint8_t>> receivedHashes;
    std::vector<std::vector<uint8_t>> receivedSignatures;

    SignatureError begin() override {
        ++this->beginCalls;
        return this->beginError;
    }

    SignatureError append(SignatureBytes data) override {
        ++this->appendCalls;
        this->receivedData.push_back(copyBytes(data));
        return this->appendError;
    }

    SignatureResult finish(
        SignatureBytes expectedHash, SignatureBytes signature) override {
        ++this->finishCalls;
        this->receivedHashes.push_back(copyBytes(expectedHash));
        this->receivedSignatures.push_back(copyBytes(signature));
        return this->finishResult;
    }

    void abort() override { ++this->abortCalls; }
};

UploadStatus makePendingTrial(
    UploadPhase phase, const std::string& candidate, uint32_t version = 12) {
    UploadStatus state;
    state.sequence = 8;
    state.activeSlot = ConfigSlot::primary;
    state.activeVersion = 11;
    state.phase = phase;
    state.trialSlot = ConfigSlot::secondary;
    state.trialVersion = version;
    state.trialHash = streamHash;
    state.trialBytes = static_cast<uint32_t>(candidate.size());
    state.minimumAcceptedConfigVersion = version + 1;
    state.lastOutcome =
        OtaResult{"STAGED", state.trialBytes, streamHash, version, ""};
    return state;
}

void seedRecord(
    FakeOtaFileSystem& fileSystem, const char* path,
    const UploadStatus& status) {
    fileSystem.seed(path, encodeUploadStatus(status));
}

std::optional<UploadStatus> decodeRecord(
    const FakeOtaFileSystem& fileSystem, const char* path) {
    const std::optional<std::string> contents = fileSystem.contents(path);
    if (!contents.has_value()) {
        return std::nullopt;
    }
    UploadStatus state;
    if (!decodeUploadStatus(*contents, state)) {
        return std::nullopt;
    }
    return state;
}

class OtaHarness {
public:
    FakeEspApi esp;
    FakeOtaFileSystem fileSystem;
    UploadJournal journal;
    ConfigBootCoordinator boot;
    MqttTransportState transport{true, 0};
    FakeVerifier* verifier = nullptr;
    std::vector<OtaResult> results;
    std::unique_ptr<DeviceConfigOta> ota;
    ConfigBootSelection selection;

    explicit OtaHarness(
        bool liveTrial = false, UploadPhase trialPhase = UploadPhase::pending,
        std::string candidate = makeConfig(12), bool exhausted = false,
        uint32_t confirmedVersion = 11)
        : journal(this->fileSystem), boot(this->fileSystem, this->journal) {
        this->fileSystem.seed(primaryPath, makeConfig(confirmedVersion));
        this->fileSystem.seed(secondaryPath, candidate);
        if (liveTrial) {
            UploadStatus trial = makePendingTrial(trialPhase, candidate);
            if (exhausted) {
                trial.minimumAcceptedConfigVersion.reset();
            }
            seedRecord(this->fileSystem, journalPath, trial);
            seedRecord(this->fileSystem, backupPath, trial);
        }
        this->selection = this->boot.selectForBoot();
        this->createOta();
    }

    void createOta() {
        auto ownedVerifier = std::make_unique<FakeVerifier>();
        this->verifier = ownedVerifier.get();
        this->ota = std::make_unique<DeviceConfigOta>(
            this->esp, this->fileSystem, this->boot, std::move(ownedVerifier),
            stationMac, [this]() { return this->transport; },
            [this](const OtaResult& result) {
            this->results.push_back(result);
        });
    }

    void send(const std::string& command, bool retained = false) {
        this->ota->execute(command, retained);
        this->ota->update();
    }

    void sendEnvelope(
        const std::string& envelope, const std::string& hash = "") {
        const std::string actualHash =
            hash.empty() ? sha256Hex(envelope) : hash;
        const std::vector<uint8_t> bytes = bytesOf(envelope);
        this->send(
            beginCommand(actualHash, static_cast<uint32_t>(bytes.size())));
        this->send(chunkCommand(actualHash, 0, bytes));
        this->send(finishCommand(actualHash));
    }
};

class RecordingOtaFileSystem;

class RecordingOtaFile final : public OtaFile {
public:
    RecordingOtaFile(
        RecordingOtaFileSystem& fileSystem, std::string path,
        std::unique_ptr<OtaFile> file);
    int read() override;
    size_t write(SignatureBytes bytes) override;
    bool close() override;
    bool failed() const override;

private:
    RecordingOtaFileSystem& fileSystem;
    std::string path;
    std::unique_ptr<OtaFile> file;
};

class RecordingOtaFileSystem final : public OtaFileSystem {
public:
    explicit RecordingOtaFileSystem(FakeOtaFileSystem& fileSystem)
        : fileSystem(fileSystem) {}

    OtaFilePresence inspect(const char* path) override {
        return this->fileSystem.inspect(path);
    }

    std::unique_ptr<OtaFile> open(const char* path, OtaFileMode mode) override {
        if (mode == OtaFileMode::read && this->mutationPath == path) {
            const std::optional<std::string> existing =
                this->fileSystem.contents(path);
            if (existing.has_value()) {
                const std::size_t offset = existing->find(this->mutationFrom);
                if (offset != std::string::npos &&
                    this->mutationFrom.size() == this->mutationTo.size()) {
                    std::string mutated = *existing;
                    mutated.replace(
                        offset, this->mutationFrom.size(), this->mutationTo);
                    this->fileSystem.seed(path, std::move(mutated));
                    this->mutationApplied = true;
                    this->mutationPath.clear();
                }
            }
        }
        std::unique_ptr<OtaFile> file = this->fileSystem.open(path, mode);
        if (file == nullptr) {
            return nullptr;
        }
        return std::make_unique<RecordingOtaFile>(*this, path, std::move(file));
    }

    bool remove(const char* path) override {
        return this->fileSystem.remove(path);
    }

    void recordWrite(const std::string& path, size_t bytes) {
        this->writes.emplace_back(path, bytes);
    }

    void mutateOnNextRead(
        const std::string& path, std::string from, std::string to) {
        this->mutationPath = path;
        this->mutationFrom = std::move(from);
        this->mutationTo = std::move(to);
    }

    std::vector<std::pair<std::string, size_t>> writes;
    bool mutationApplied = false;

private:
    FakeOtaFileSystem& fileSystem;
    std::string mutationPath;
    std::string mutationFrom;
    std::string mutationTo;
};

RecordingOtaFile::RecordingOtaFile(
    RecordingOtaFileSystem& fileSystem, std::string path,
    std::unique_ptr<OtaFile> file)
    : fileSystem(fileSystem), path(std::move(path)), file(std::move(file)) {}

int RecordingOtaFile::read() {
    return this->file->read();
}

size_t RecordingOtaFile::write(SignatureBytes bytes) {
    this->fileSystem.recordWrite(this->path, bytes.size);
    return this->file->write(bytes);
}

bool RecordingOtaFile::close() {
    return this->file->close();
}

bool RecordingOtaFile::failed() const {
    return this->file->failed();
}

MqttReadiness runtimeReadiness(
    MqttReadiness mqttReadiness, bool certificateValid,
    bool subscriptionReady) {
    if (mqttReadiness == MqttReadiness::collision) {
        return MqttReadiness::collision;
    }
    return mqttReadiness == MqttReadiness::ready && certificateValid &&
                   subscriptionReady
               ? MqttReadiness::ready
               : MqttReadiness::checking;
}

std::size_t resultCount(const OtaHarness& harness, const std::string& status) {
    return static_cast<std::size_t>(std::count_if(
        harness.results.begin(), harness.results.end(),
        [&status](const OtaResult& result) {
        return result.status == status;
    }));
}

const OtaResult* lastResult(
    const OtaHarness& harness, const std::string& status) {
    const auto found = std::find_if(
        harness.results.rbegin(), harness.results.rend(),
        [&status](const OtaResult& result) { return result.status == status; });
    return found == harness.results.rend() ? nullptr : &*found;
}

void expectNoStagedResult(const OtaHarness& harness) {
    EXPECT_EQ(resultCount(harness, "STAGED"), 0u);
}

void expectIdleConfirmedState(const OtaHarness& harness) {
    EXPECT_EQ(harness.boot.snapshot().configVersion, 11u);
    EXPECT_EQ(harness.boot.snapshot().phase, UploadPhase::idle);
    EXPECT_EQ(
        harness.boot.snapshot().minimumAcceptedConfigVersion,
        std::optional<uint32_t>{12});
}

void sendEnvelopeUntilChunk(
    OtaHarness& harness, const std::string& envelope, std::size_t chunkLength,
    const std::string& hash = streamHash) {
    const std::vector<uint8_t> bytes = bytesOf(envelope);
    harness.send(beginCommand(hash, static_cast<uint32_t>(bytes.size())));
    std::vector<uint8_t> partial;
    partial.reserve(chunkLength);
    for (std::size_t index = 0; index < std::min(chunkLength, bytes.size());
         ++index) {
        partial.push_back(bytes[index]);
    }
    harness.send(chunkCommand(hash, 0, partial));
}

void expectNotTrialAndConfirmed(
    FakeOtaFileSystem& fileSystem, uint32_t version = 11) {
    fileSystem.reboot();
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);
    const ConfigBootSelection selection = boot.selectForBoot();
    EXPECT_EQ(selection.slot, ConfigSlot::primary);
    EXPECT_FALSE(selection.trial);
    EXPECT_EQ(boot.snapshot().configVersion, version);
    EXPECT_EQ(boot.snapshot().phase, UploadPhase::idle);
}

enum class StorageFault {
    envelopeOpen,
    envelopeWrite,
    envelopeClose,
    envelopeReadOpen,
    envelopeRead,
    candidateWrite,
    candidateReadback,
    cleanup,
    journal
};

void setStorageFault(
    OtaHarness& harness, StorageFault fault, bool enabled = true) {
    switch (fault) {
    case StorageFault::envelopeOpen:
    case StorageFault::envelopeReadOpen:
        harness.fileSystem.setOpenError(downloadPath, enabled);
        break;
    case StorageFault::envelopeWrite:
    case StorageFault::candidateWrite:
        if (enabled) {
            harness.fileSystem.shortWriteAfter(0);
        }
        break;
    case StorageFault::envelopeClose:
        harness.fileSystem.setCloseFailure(downloadPath, enabled);
        break;
    case StorageFault::envelopeRead:
        harness.fileSystem.setReadErrorAfter(
            downloadPath,
            enabled ? 0 : std::numeric_limits<std::size_t>::max());
        break;
    case StorageFault::candidateReadback:
        harness.fileSystem.setReadErrorAfter(
            secondaryPath,
            enabled ? 0 : std::numeric_limits<std::size_t>::max());
        break;
    case StorageFault::cleanup:
        harness.fileSystem.setInspectError(downloadPath, enabled);
        break;
    case StorageFault::journal:
        harness.fileSystem.setCloseFailure(backupPath, enabled);
        break;
    }
}

}  // namespace

TEST(Sha256Test, MatchesKnownVector) {
    EXPECT_EQ(
        sha256Hex("abc"),
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(
        sha256Hex(std::string(100, 'a')),
        "2816597888e4a0d3a36b82b83316ab32680eb8f00f8cd3b904d681246d285a0e");
    EXPECT_EQ(streamHash, sha256Hex(makeEnvelope(makeConfig(12))));
}

TEST(DeviceConfigOtaTest, NoBootableCandidateBeforeValidSignature) {
    OtaHarness harness;
    const std::string candidate = makeConfig(12, "home/new");
    const std::string envelope = makeEnvelope(candidate);
    const std::optional<std::string> originalPrimary =
        harness.fileSystem.contents(primaryPath);
    const std::optional<std::string> originalCandidate =
        harness.fileSystem.contents(secondaryPath);

    const std::vector<uint8_t> bytes = bytesOf(envelope);
    harness.send(beginCommand(streamHash, static_cast<uint32_t>(bytes.size())));
    harness.send(chunkCommand(streamHash, 0, bytes));
    harness.verifier->finishResult = {
        SignatureStatus::invalid, SignatureError::invalidSignature};
    harness.send(finishCommand(streamHash));

    EXPECT_EQ(harness.fileSystem.contents(primaryPath), originalPrimary);
    EXPECT_EQ(harness.fileSystem.contents(secondaryPath), originalCandidate);
    EXPECT_FALSE(harness.fileSystem.contents(downloadPath).has_value());
    expectIdleConfirmedState(harness);
    expectNoStagedResult(harness);
    expectNotTrialAndConfirmed(harness.fileSystem);
}

TEST(DeviceConfigOtaTest, ExhaustedVersionRequestKeepsBeginHashAndExactReason) {
    OtaHarness harness(
        false, UploadPhase::pending, makeConfig(UINT32_MAX), false, UINT32_MAX);

    harness.send(beginCommand(streamHash, 96));

    const OtaResult* failure = lastResult(harness, "ERROR");
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->reason, "version-range-exhausted");
    EXPECT_EQ(failure->hash, streamHash);
    EXPECT_EQ(failure->transferred, 0u);
    EXPECT_EQ(
        encodeOtaResult(*failure),
        R"({"status":"ERROR","transferred":0,"hash":")" + streamHash +
            R"(","version":null,"reason":"version-range-exhausted"})");
}

TEST(DeviceConfigOtaTest, BeginSizeLimitRejectsBeforeOpenAndAcceptsBoundary) {
    constexpr uint32_t maxDocumentBytes = 32768;
    {
        OtaHarness harness;
        const std::size_t operationStart =
            harness.fileSystem.operationLog().size();

        harness.send(beginCommand(streamHash, maxDocumentBytes + 1));

        const OtaResult* failure = lastResult(harness, "ERROR");
        ASSERT_NE(failure, nullptr);
        EXPECT_EQ(failure->reason, "document-too-large");
        EXPECT_EQ(failure->hash, streamHash);
        EXPECT_FALSE(harness.fileSystem.contents(downloadPath).has_value());
        const auto& operations = harness.fileSystem.operationLog();
        EXPECT_EQ(
            std::find(
                operations.begin() +
                    static_cast<std::ptrdiff_t>(operationStart),
                operations.end(), std::string("open-truncate:") + downloadPath),
            operations.end());
        EXPECT_EQ(
            std::find(
                operations.begin() +
                    static_cast<std::ptrdiff_t>(operationStart),
                operations.end(), std::string("write:") + downloadPath),
            operations.end());
    }

    {
        OtaHarness harness;
        harness.send(beginCommand(streamHash, maxDocumentBytes));

        const OtaResult* progress = lastResult(harness, "PROGRESS");
        ASSERT_NE(progress, nullptr);
        EXPECT_EQ(progress->hash, streamHash);
        ASSERT_TRUE(harness.fileSystem.contents(downloadPath).has_value());
        EXPECT_TRUE(harness.fileSystem.contents(downloadPath)->empty());
        EXPECT_NE(
            std::find(
                harness.fileSystem.operationLog().begin(),
                harness.fileSystem.operationLog().end(),
                std::string("open-truncate:") + downloadPath),
            harness.fileSystem.operationLog().end());
    }
}

TEST(DeviceConfigOtaTest, RejectsWrongMacTypeVersionOrDisabledOta) {
    OtaHarness harness;
    const std::string unchangedCandidate =
        *harness.fileSystem.contents(secondaryPath);
    const std::vector<std::string> invalidEnvelopes = {
        makeEnvelope(makeConfig(12), "AA:BB:CC:DD:EE:00"),
        makeEnvelope(makeConfig(12), stationMac, "firmware"),
        makeEnvelope(makeConfig(11)),
        makeEnvelope(makeConfig(12, "home/device", false))};

    for (const std::string& envelope : invalidEnvelopes) {
        SCOPED_TRACE(envelope);
        harness.sendEnvelope(envelope);
        EXPECT_FALSE(harness.fileSystem.contents(downloadPath).has_value());
        EXPECT_EQ(
            harness.fileSystem.contents(secondaryPath),
            std::optional<std::string>{unchangedCandidate});
        expectIdleConfirmedState(harness);
    }

    expectNoStagedResult(harness);
}

TEST(DeviceConfigOtaTest, PartialDownloadNeverTouchesConfirmedSlot) {
    OtaHarness harness;
    const std::string envelope = makeEnvelope(makeConfig(12));
    const std::optional<std::string> primaryBefore =
        harness.fileSystem.contents(primaryPath);
    const std::optional<std::string> candidateBefore =
        harness.fileSystem.contents(secondaryPath);
    const std::size_t partialLength = envelope.size() / 2;

    sendEnvelopeUntilChunk(harness, envelope, partialLength);

    ASSERT_TRUE(harness.fileSystem.contents(downloadPath).has_value());
    EXPECT_EQ(
        harness.fileSystem.contents(downloadPath),
        std::optional<std::string>{envelope.substr(0, partialLength)});
    EXPECT_EQ(harness.fileSystem.contents(primaryPath), primaryBefore);
    EXPECT_EQ(harness.fileSystem.contents(secondaryPath), candidateBefore);
    expectIdleConfirmedState(harness);
    expectNoStagedResult(harness);

    harness.send(finishCommand(streamHash));
    EXPECT_FALSE(harness.fileSystem.contents(downloadPath).has_value());
    EXPECT_EQ(harness.fileSystem.contents(primaryPath), primaryBefore);
    EXPECT_EQ(harness.fileSystem.contents(secondaryPath), candidateBefore);
}

TEST(DeviceConfigOtaTest, StagedOnlyAfterCandidateReadbackAndMirroredFloor) {
    OtaHarness harness;
    const std::string candidate = makeConfig(12, "home/new");
    const std::string envelope = makeEnvelope(candidate);
    const std::string envelopeHash = sha256Hex(envelope);
    const std::vector<uint8_t> signedBytes = bytesOf(envelope);
    const std::size_t transactionStart =
        harness.fileSystem.operationLog().size();

    harness.sendEnvelope(envelope);

    EXPECT_EQ(
        harness.fileSystem.contents(primaryPath),
        std::optional<std::string>{makeConfig(11)});
    const std::optional<std::string> installed =
        harness.fileSystem.contents(secondaryPath);
    ASSERT_TRUE(installed.has_value());
    EXPECT_EQ(*installed, candidate);
    EXPECT_EQ(installed->find(R"("type")"), std::string::npos);
    EXPECT_EQ(installed->find(R"("target")"), std::string::npos);
    ArduinoJson::DynamicJsonBuffer configBuffer{1024};
    ArduinoJson::JsonObject& installedConfig =
        configBuffer.parseObject(*installed);
    ASSERT_NE(installedConfig, ArduinoJson::JsonObject::invalid());
    EXPECT_EQ(installedConfig.get<uint32_t>("configVersion"), 12u);
    EXPECT_EQ(installedConfig.get<std::string>("servicePrefix"), "home/new");

    ASSERT_EQ(harness.verifier->receivedData.size(), 1u);
    EXPECT_EQ(harness.verifier->receivedData[0], signedBytes);
    ASSERT_EQ(harness.verifier->receivedHashes.size(), 1u);
    EXPECT_EQ(harness.verifier->receivedHashes[0], hashBytes(envelopeHash));
    ASSERT_EQ(harness.verifier->receivedSignatures.size(), 1u);
    EXPECT_EQ(
        harness.verifier->receivedSignatures[0],
        std::vector<uint8_t>(64, 0xA5));

    const std::optional<UploadStatus> primary =
        decodeRecord(harness.fileSystem, journalPath);
    const std::optional<UploadStatus> backup =
        decodeRecord(harness.fileSystem, backupPath);
    ASSERT_TRUE(primary.has_value());
    ASSERT_TRUE(backup.has_value());
    for (const UploadStatus* state : {&*primary, &*backup}) {
        EXPECT_EQ(state->phase, UploadPhase::pending);
        EXPECT_EQ(state->activeSlot, ConfigSlot::primary);
        EXPECT_EQ(state->trialSlot, ConfigSlot::secondary);
        EXPECT_EQ(state->trialVersion, std::optional<uint32_t>{12});
        EXPECT_EQ(state->trialHash, envelopeHash);
        EXPECT_EQ(
            state->minimumAcceptedConfigVersion, std::optional<uint32_t>{13});
        ASSERT_TRUE(state->lastOutcome.has_value());
        EXPECT_EQ(state->lastOutcome->status, "STAGED");
    }
    EXPECT_EQ(primary->sequence, backup->sequence);

    const std::vector<std::string>& operations =
        harness.fileSystem.operationLog();
    const auto candidateRead = std::find(
        operations.begin() + static_cast<std::ptrdiff_t>(transactionStart),
        operations.end(), std::string("open-read:") + secondaryPath);
    const auto firstJournalWrite = std::find(
        operations.begin() + static_cast<std::ptrdiff_t>(transactionStart),
        operations.end(), std::string("open-truncate:") + journalPath);
    const auto mirroredJournalWrite = std::find(
        operations.begin() + static_cast<std::ptrdiff_t>(transactionStart),
        operations.end(), std::string("open-truncate:") + backupPath);
    ASSERT_NE(candidateRead, operations.end());
    ASSERT_NE(firstJournalWrite, operations.end());
    ASSERT_NE(mirroredJournalWrite, operations.end());
    EXPECT_LT(candidateRead, firstJournalWrite);
    EXPECT_LT(firstJournalWrite, mirroredJournalWrite);
    EXPECT_FALSE(harness.fileSystem.contents(downloadPath).has_value());
    EXPECT_EQ(harness.fileSystem.bytesRead(downloadPath), envelope.size());
}

TEST(DeviceConfigOtaTest, ConfigSerializationWritesBoundedChunksAndExactBytes) {
    constexpr std::size_t maxWriterChunk = 256;
    const std::string candidate = makeConfig(
        12, "home/new", true,
        R"(,"padding":")" + std::string(400, 'x') + R"(")");
    const std::string envelope = makeEnvelope(candidate);
    const std::string envelopeHash = sha256Hex(envelope);
    const std::vector<uint8_t> bytes = bytesOf(envelope);
    FakeOtaFileSystem backingFileSystem;
    RecordingOtaFileSystem fileSystem(backingFileSystem);
    FakeEspApi esp;
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);
    MqttTransportState transport{true, 0};
    std::vector<OtaResult> results;
    auto verifier = std::make_unique<FakeVerifier>();
    backingFileSystem.seed(primaryPath, makeConfig(11));
    backingFileSystem.seed(secondaryPath, makeConfig(10));
    const ConfigBootSelection selection = boot.selectForBoot();
    ASSERT_EQ(selection.slot, ConfigSlot::primary);
    ASSERT_TRUE(selection.otaAllowed);

    DeviceConfigOta ota(
        esp, fileSystem, boot, std::move(verifier), stationMac, [&transport]() {
        return transport;
    }, [&results](const OtaResult& result) { results.push_back(result); });
    auto send = [&ota](const std::string& command) {
        ota.execute(command, false);
        ota.update();
    };

    send(beginCommand(envelopeHash, static_cast<uint32_t>(bytes.size())));
    for (std::size_t offset = 0; offset < bytes.size();) {
        const std::size_t count =
            std::min<std::size_t>(256, bytes.size() - offset);
        const std::vector<uint8_t> chunk(
            bytes.begin() + static_cast<std::ptrdiff_t>(offset),
            bytes.begin() + static_cast<std::ptrdiff_t>(offset + count));
        send(chunkCommand(envelopeHash, static_cast<uint32_t>(offset), chunk));
        offset += count;
    }
    send(finishCommand(envelopeHash));

    ASSERT_GT(candidate.size(), maxWriterChunk);
    EXPECT_EQ(
        backingFileSystem.contents(secondaryPath),
        std::optional<std::string>{candidate});
    std::size_t totalCandidateWrites = 0;
    for (const auto& [path, bytesWritten] : fileSystem.writes) {
        if (path == secondaryPath) {
            EXPECT_LE(bytesWritten, maxWriterChunk);
            totalCandidateWrites += bytesWritten;
        }
    }
    EXPECT_EQ(totalCandidateWrites, candidate.size());
    EXPECT_EQ(
        std::count_if(
            results.begin(), results.end(),
            [](const OtaResult& result) { return result.status == "STAGED"; }),
        1);
}

TEST(DeviceConfigOtaTest, SameVersionReadbackMutationDoesNotStageCandidate) {
    const std::string candidate = makeConfig(12, "home/new");
    const std::string mutatedCandidate = makeConfig(12, "evil/new");
    FakeOtaFileSystem backingFileSystem;
    RecordingOtaFileSystem fileSystem(backingFileSystem);
    FakeEspApi esp;
    UploadJournal journal(fileSystem);
    ConfigBootCoordinator boot(fileSystem, journal);
    MqttTransportState transport{true, 0};
    std::vector<OtaResult> results;
    auto verifier = std::make_unique<FakeVerifier>();
    backingFileSystem.seed(primaryPath, makeConfig(11));
    backingFileSystem.seed(secondaryPath, makeConfig(10));
    const ConfigBootSelection selection = boot.selectForBoot();
    ASSERT_EQ(selection.slot, ConfigSlot::primary);
    ASSERT_TRUE(selection.otaAllowed);

    DeviceConfigOta ota(
        esp, fileSystem, boot, std::move(verifier), stationMac, [&transport]() {
        return transport;
    }, [&results](const OtaResult& result) { results.push_back(result); });
    auto send = [&ota](const std::string& command) {
        ota.execute(command, false);
        ota.update();
    };

    const std::string envelope = makeEnvelope(candidate);
    const std::string envelopeHash = sha256Hex(envelope);
    const std::vector<uint8_t> bytes = bytesOf(envelope);
    send(beginCommand(envelopeHash, static_cast<uint32_t>(bytes.size())));
    send(chunkCommand(envelopeHash, 0, bytes));
    fileSystem.mutateOnNextRead(secondaryPath, "home/new", "evil/new");
    send(finishCommand(envelopeHash));

    EXPECT_TRUE(fileSystem.mutationApplied);
    EXPECT_EQ(
        backingFileSystem.contents(secondaryPath),
        std::optional<std::string>{mutatedCandidate});
    EXPECT_EQ(
        std::count_if(
            results.begin(), results.end(),
            [](const OtaResult& result) { return result.status == "STAGED"; }),
        0);
    const auto failure = std::find_if(
        results.rbegin(), results.rend(),
        [](const OtaResult& result) { return result.status == "ERROR"; });
    ASSERT_NE(failure, results.rend());
    EXPECT_EQ(failure->reason, "candidate-write-failed");
    EXPECT_EQ(failure->hash, envelopeHash);
    EXPECT_EQ(boot.snapshot().phase, UploadPhase::idle);
    EXPECT_EQ(boot.snapshot().configVersion, 11u);
    EXPECT_FALSE(esp.restarted);

    expectNotTrialAndConfirmed(backingFileSystem);
}

TEST(DeviceConfigOtaTest, DownloadMutationAfterVerificationDoesNotStage) {
    OtaHarness harness;
    const std::string candidate = makeConfig(12, "home/new");
    const std::string mutatedCandidate = makeConfig(12, "evil/new");
    harness.fileSystem.mutateOnNextReadOpen(
        downloadPath, "home/new", "evil/new");

    harness.sendEnvelope(makeEnvelope(candidate));

    EXPECT_TRUE(harness.fileSystem.readOpenMutationApplied());
    EXPECT_EQ(
        harness.fileSystem.contents(downloadPath),
        std::optional<std::string>{makeEnvelope(mutatedCandidate)});
    EXPECT_EQ(
        harness.fileSystem.bytesRead(downloadPath),
        makeEnvelope(mutatedCandidate).size());
    EXPECT_EQ(resultCount(harness, "STAGED"), 0u);
    const OtaResult* error = lastResult(harness, "ERROR");
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->status, "ERROR");
    EXPECT_EQ(
        harness.fileSystem.contents(secondaryPath),
        std::optional<std::string>{makeConfig(12)});
    EXPECT_EQ(harness.boot.snapshot().phase, UploadPhase::idle);
    EXPECT_EQ(harness.boot.snapshot().configVersion, 11u);
    EXPECT_EQ(
        harness.boot.snapshot().minimumAcceptedConfigVersion,
        std::optional<uint32_t>{12});
    ASSERT_TRUE(harness.journal.state().has_value());
    EXPECT_EQ(harness.journal.state()->activeSlot, ConfigSlot::primary);
    EXPECT_EQ(harness.journal.state()->activeVersion, 11u);
    EXPECT_EQ(
        harness.journal.state()->minimumAcceptedConfigVersion,
        std::optional<uint32_t>{12});
}

TEST(DeviceConfigOtaTest, DownloadLengthMutationDoesNotStage) {
    OtaHarness harness;
    const std::string candidate = makeConfig(12, "home/new");
    const std::string envelope = makeEnvelope(candidate);
    const std::string mutatedEnvelope =
        envelope.substr(0, envelope.size() - 2) + "} } ";
    harness.fileSystem.mutateOnNextReadOpen(downloadPath, "}}", "} } ");

    harness.sendEnvelope(envelope);

    EXPECT_TRUE(harness.fileSystem.readOpenMutationApplied());
    EXPECT_EQ(
        harness.fileSystem.contents(downloadPath),
        std::optional<std::string>{mutatedEnvelope});
    EXPECT_EQ(
        harness.fileSystem.bytesRead(downloadPath), mutatedEnvelope.size());
    EXPECT_EQ(resultCount(harness, "STAGED"), 0u);
    const OtaResult* error = lastResult(harness, "ERROR");
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->reason, "envelope-readback-mismatch");
    EXPECT_EQ(
        harness.fileSystem.contents(secondaryPath),
        std::optional<std::string>{makeConfig(12)});
}

TEST(DeviceConfigOtaTest, RejectsRetainedCommandsAndTrialUpdates) {
    OtaHarness confirmed;
    const std::string envelope = makeEnvelope(makeConfig(12));
    const std::vector<uint8_t> bytes = bytesOf(envelope);
    confirmed.send(
        beginCommand(streamHash, static_cast<uint32_t>(bytes.size())), true);
    EXPECT_EQ(confirmed.verifier->beginCalls, 0u);
    EXPECT_FALSE(confirmed.fileSystem.contents(downloadPath).has_value());
    expectIdleConfirmedState(confirmed);

    OtaHarness trial(true);
    ASSERT_TRUE(trial.selection.trial);
    ASSERT_TRUE(trial.boot.trialRunning());
    trial.send(beginCommand(streamHash, static_cast<uint32_t>(bytes.size())));
    EXPECT_EQ(trial.verifier->beginCalls, 0u);
    EXPECT_FALSE(trial.fileSystem.contents(downloadPath).has_value());
    EXPECT_TRUE(trial.boot.trialRunning());
    EXPECT_EQ(trial.boot.snapshot().phase, UploadPhase::started);
}

TEST(DeviceConfigOtaTest, PrefixChangeKeepsHashCorrelation) {
    OtaHarness harness;
    const std::string envelope = makeEnvelope(makeConfig(12, "other/site"));
    const std::string envelopeHash = sha256Hex(envelope);
    harness.sendEnvelope(envelope);

    const OtaResult* staged = lastResult(harness, "STAGED");
    ASSERT_NE(staged, nullptr);
    EXPECT_EQ(staged->hash, envelopeHash);
    EXPECT_EQ(staged->version, std::optional<uint32_t>{12});
    EXPECT_EQ(resultCount(harness, "STAGED"), 1u);
    ASSERT_FALSE(harness.results.empty());
    EXPECT_EQ(harness.results.front().status, "PROGRESS");
    EXPECT_EQ(harness.results.front().hash, envelopeHash);
    EXPECT_FALSE(harness.results.front().version.has_value());
    for (const OtaResult& result : harness.results) {
        if (!result.hash.empty()) {
            EXPECT_EQ(result.hash, envelopeHash);
        }
    }
}

TEST(DeviceConfigOtaTest, ReadinessRequiredForConfirmation) {
    OtaHarness harness(true);
    ASSERT_TRUE(harness.selection.trial);
    ASSERT_TRUE(harness.boot.trialRunning());

    harness.ota->observeMqtt(MqttReadiness::checking, false);
    harness.ota->update();
    EXPECT_TRUE(harness.boot.trialRunning());
    EXPECT_EQ(harness.boot.snapshot().phase, UploadPhase::started);
    EXPECT_FALSE(harness.esp.restarted);

    harness.ota->observeMqtt(MqttReadiness::ready, false);
    EXPECT_TRUE(harness.boot.trialRunning());
    harness.ota->update();
    EXPECT_FALSE(harness.boot.trialRunning());
    EXPECT_EQ(harness.boot.snapshot().phase, UploadPhase::idle);
    ASSERT_TRUE(harness.boot.snapshot().lastOutcome.has_value());
    EXPECT_EQ(harness.boot.snapshot().lastOutcome->status, "CONFIRMED");
}

TEST(
    DeviceConfigOtaTest,
    ConfirmationMirrorFailureDoesNotReportConfirmedOutcome) {
    OtaHarness harness(true);
    ASSERT_TRUE(harness.selection.trial);
    const std::size_t operationStart = harness.fileSystem.operationLog().size();
    harness.fileSystem.setCloseFailure(backupPath);

    harness.ota->observeMqtt(MqttReadiness::ready, false);
    harness.ota->update();

    EXPECT_EQ(resultCount(harness, "CONFIRMED"), 0u);
    const OtaStatusSnapshot snapshot = harness.boot.snapshot();
    EXPECT_FALSE(
        snapshot.lastOutcome.has_value() &&
        snapshot.lastOutcome->status == "CONFIRMED");
    EXPECT_FALSE(snapshot.updatesAllowed);

    const std::vector<std::string>& operations =
        harness.fileSystem.operationLog();
    const auto primaryClose = std::find(
        operations.begin() + static_cast<std::ptrdiff_t>(operationStart),
        operations.end(), std::string("close:") + journalPath);
    const auto mirrorOpen = std::find(
        operations.begin() + static_cast<std::ptrdiff_t>(operationStart),
        operations.end(), std::string("open-truncate:") + backupPath);
    const auto mirrorClose = std::find(
        operations.begin() + static_cast<std::ptrdiff_t>(operationStart),
        operations.end(), std::string("close:") + backupPath);
    ASSERT_NE(primaryClose, operations.end());
    ASSERT_NE(mirrorOpen, operations.end());
    ASSERT_NE(mirrorClose, operations.end());
    EXPECT_LT(primaryClose, mirrorOpen);
    EXPECT_LT(mirrorOpen, mirrorClose);

    const std::optional<UploadStatus> firstRecord =
        decodeRecord(harness.fileSystem, journalPath);
    ASSERT_TRUE(firstRecord.has_value());
    ASSERT_TRUE(firstRecord->lastOutcome.has_value());
    EXPECT_EQ(firstRecord->lastOutcome->status, "CONFIRMED");
}

TEST(DeviceConfigOtaTest, SelectedPathReachesBootParser) {
    OtaHarness harness(true);

    ASSERT_TRUE(harness.selection.trial);
    EXPECT_EQ(harness.selection.slot, ConfigSlot::secondary);
    EXPECT_STREQ(configSlotPath(harness.selection.slot), secondaryPath);

    const std::vector<std::string>& operations =
        harness.fileSystem.operationLog();
    EXPECT_NE(
        std::find(
            operations.begin(), operations.end(),
            std::string("open-read:") + secondaryPath),
        operations.end());
}

TEST(DeviceConfigOtaTest, MissingCertificateBlocksCandidateConfirmation) {
    OtaHarness harness(true);
    ASSERT_TRUE(harness.selection.trial);

    const MqttReadiness readiness =
        runtimeReadiness(MqttReadiness::ready, false, true);
    EXPECT_EQ(readiness, MqttReadiness::checking);
    harness.ota->observeMqtt(readiness, false);
    harness.ota->update();

    EXPECT_TRUE(harness.boot.trialRunning());
    EXPECT_EQ(harness.boot.snapshot().phase, UploadPhase::started);
    EXPECT_FALSE(harness.esp.restarted);

    ASSERT_TRUE(harness.boot.rollback("certificate-invalid"));
    EXPECT_FALSE(harness.boot.trialRunning());
    ASSERT_TRUE(harness.boot.snapshot().lastOutcome.has_value());
    EXPECT_EQ(harness.boot.snapshot().lastOutcome->status, "ROLLED_BACK");
    EXPECT_EQ(
        harness.boot.snapshot().lastOutcome->reason, "certificate-invalid");

    harness.fileSystem.reboot();
    UploadJournal recoveredJournal(harness.fileSystem);
    ConfigBootCoordinator recoveredBoot(harness.fileSystem, recoveredJournal);
    const ConfigBootSelection recovered = recoveredBoot.selectForBoot();
    EXPECT_EQ(recovered.slot, ConfigSlot::primary);
    EXPECT_FALSE(recovered.trial);
    EXPECT_EQ(recoveredBoot.snapshot().configVersion, 11u);
}

TEST(DeviceConfigOtaTest, SubscriptionFailureKeepsTrialUnconfirmed) {
    OtaHarness harness(true);
    ASSERT_TRUE(harness.selection.trial);

    const MqttReadiness readiness =
        runtimeReadiness(MqttReadiness::ready, true, false);
    EXPECT_EQ(readiness, MqttReadiness::checking);
    harness.ota->observeMqtt(readiness, false);
    harness.ota->update();

    EXPECT_TRUE(harness.boot.trialRunning());
    EXPECT_EQ(harness.boot.snapshot().phase, UploadPhase::started);
    EXPECT_FALSE(harness.esp.restarted);
    ASSERT_TRUE(harness.boot.snapshot().lastOutcome.has_value());
    EXPECT_EQ(harness.boot.snapshot().lastOutcome->status, "STAGED");

    OtaHarness exhausted(true, UploadPhase::pending, makeConfig(12), true);
    ASSERT_TRUE(exhausted.selection.trial);
    EXPECT_FALSE(
        exhausted.boot.snapshot().minimumAcceptedConfigVersion.has_value());
    const MqttReadiness exhaustedReadiness =
        runtimeReadiness(MqttReadiness::ready, true, true);
    EXPECT_EQ(exhaustedReadiness, MqttReadiness::ready);
    exhausted.ota->observeMqtt(exhaustedReadiness, false);
    exhausted.ota->update();

    EXPECT_FALSE(exhausted.boot.trialRunning());
    EXPECT_EQ(exhausted.boot.snapshot().phase, UploadPhase::idle);
    ASSERT_TRUE(exhausted.boot.snapshot().lastOutcome.has_value());
    EXPECT_EQ(exhausted.boot.snapshot().lastOutcome->status, "CONFIRMED");
    EXPECT_EQ(exhausted.boot.snapshot().configVersion, 12u);
}

TEST(DeviceConfigOtaTest, CollisionWinsOverReadyInSameTick) {
    OtaHarness harness(true);
    ASSERT_TRUE(harness.selection.trial);

    harness.ota->observeMqtt(MqttReadiness::ready, true);
    EXPECT_TRUE(harness.boot.trialRunning());
    EXPECT_FALSE(harness.esp.restarted);
    harness.ota->update();

    EXPECT_FALSE(harness.boot.trialRunning());
    EXPECT_EQ(harness.boot.snapshot().phase, UploadPhase::idle);
    EXPECT_EQ(harness.boot.snapshot().configVersion, 11u);
    EXPECT_EQ(
        harness.boot.snapshot().minimumAcceptedConfigVersion,
        std::optional<uint32_t>{13});
    ASSERT_TRUE(harness.boot.snapshot().lastOutcome.has_value());
    EXPECT_EQ(harness.boot.snapshot().lastOutcome->status, "ROLLED_BACK");
    EXPECT_EQ(
        harness.boot.snapshot().lastOutcome->reason, "candidate-collision");
    EXPECT_TRUE(harness.esp.restarted);
}

TEST(DeviceConfigOtaTest, CollisionRollbackPersistenceFailureRequestsRestart) {
    OtaHarness harness(true);
    ASSERT_TRUE(harness.selection.trial);
    harness.fileSystem.setCloseFailure(backupPath);

    harness.ota->observeMqtt(MqttReadiness::collision, true);
    harness.ota->update();

    EXPECT_TRUE(harness.esp.restarted);
    EXPECT_FALSE(harness.boot.trialRunning());
    EXPECT_EQ(resultCount(harness, "ROLLED_BACK"), 0u);

    harness.fileSystem.reboot();
    UploadJournal rebootedJournal(harness.fileSystem);
    ConfigBootCoordinator rebootedBoot(harness.fileSystem, rebootedJournal);
    const ConfigBootSelection selection = rebootedBoot.selectForBoot();

    EXPECT_EQ(selection.slot, ConfigSlot::primary);
    EXPECT_FALSE(selection.trial);
    EXPECT_EQ(rebootedBoot.snapshot().configVersion, 11u);
    EXPECT_EQ(
        rebootedBoot.snapshot().minimumAcceptedConfigVersion,
        std::optional<uint32_t>{13});
}

TEST(DeviceConfigOtaTest, NetworkOutageLeavesLiveTrial) {
    OtaHarness harness(true);
    ASSERT_TRUE(harness.selection.trial);

    harness.transport = {false, 1};
    harness.ota->observeMqtt(MqttReadiness::checking, false);
    harness.ota->update();

    EXPECT_TRUE(harness.boot.trialRunning());
    EXPECT_EQ(harness.boot.snapshot().phase, UploadPhase::started);
    EXPECT_FALSE(harness.esp.restarted);
    ASSERT_TRUE(harness.journal.state().has_value());
    EXPECT_EQ(harness.journal.state()->phase, UploadPhase::started);
}

TEST(DeviceConfigOtaTest, PersistedOutcomeSurvivesReboot) {
    OtaHarness harness(true);
    ASSERT_TRUE(harness.selection.trial);
    harness.ota->observeMqtt(MqttReadiness::ready, false);
    harness.ota->update();
    ASSERT_TRUE(harness.boot.snapshot().lastOutcome.has_value());
    ASSERT_EQ(harness.boot.snapshot().lastOutcome->status, "CONFIRMED");

    harness.fileSystem.reboot();
    UploadJournal rebootedJournal(harness.fileSystem);
    ConfigBootCoordinator rebootedBoot(harness.fileSystem, rebootedJournal);
    const ConfigBootSelection selection = rebootedBoot.selectForBoot();

    EXPECT_EQ(selection.slot, ConfigSlot::secondary);
    EXPECT_FALSE(selection.trial);
    EXPECT_EQ(rebootedBoot.snapshot().phase, UploadPhase::idle);
    ASSERT_TRUE(rebootedBoot.snapshot().lastOutcome.has_value());
    EXPECT_EQ(rebootedBoot.snapshot().lastOutcome->status, "CONFIRMED");
    EXPECT_EQ(rebootedBoot.snapshot().lastOutcome->hash, streamHash);
    EXPECT_EQ(
        rebootedBoot.snapshot().lastOutcome->version,
        std::optional<uint32_t>{12});
}

TEST(DeviceConfigOtaTest, SignatureAndHashFailuresRemoveTemporaryEnvelope) {
    const std::vector<SignatureResult> failures = {
        {SignatureStatus::invalid, SignatureError::invalidSignature},
        {SignatureStatus::invalid, SignatureError::hashMismatch}};

    for (const SignatureResult& failure : failures) {
        OtaHarness harness;
        harness.verifier->finishResult = failure;
        const std::string envelope = makeEnvelope(makeConfig(12));
        const std::optional<std::string> candidateBefore =
            harness.fileSystem.contents(secondaryPath);

        harness.sendEnvelope(envelope);

        EXPECT_FALSE(harness.fileSystem.contents(downloadPath).has_value());
        EXPECT_EQ(harness.fileSystem.contents(secondaryPath), candidateBefore);
        expectIdleConfirmedState(harness);
        expectNoStagedResult(harness);
        EXPECT_EQ(harness.verifier->finishCalls, 1u);
        const OtaResult* failureResult = lastResult(harness, "FAIL");
        ASSERT_NE(failureResult, nullptr);
        EXPECT_EQ(
            failureResult->reason, failure.error == SignatureError::hashMismatch
                                       ? "hash mismatch"
                                       : "invalid signature");
    }
}

TEST(DeviceConfigOtaTest, StorageShortWriteCloseAndReadbackFailuresNeverStage) {
    for (const char* failure :
         {"short-envelope", "short-candidate", "close-envelope",
          "read-envelope", "close-candidate", "read-candidate"}) {
        SCOPED_TRACE(failure);
        OtaHarness harness;
        const std::string envelope = makeEnvelope(makeConfig(12));
        const std::vector<uint8_t> bytes = bytesOf(envelope);
        const std::optional<std::string> primaryBefore =
            harness.fileSystem.contents(primaryPath);

        harness.send(
            beginCommand(streamHash, static_cast<uint32_t>(bytes.size())));
        if (std::string(failure) == "short-envelope") {
            harness.fileSystem.shortWriteAfter(0);
        }
        harness.send(chunkCommand(streamHash, 0, bytes));

        if (std::string(failure) == "short-candidate") {
            harness.fileSystem.shortWriteAfter(0);
        } else if (std::string(failure) == "close-envelope") {
            harness.fileSystem.setCloseFailure(downloadPath);
        } else if (std::string(failure) == "read-envelope") {
            harness.fileSystem.setReadErrorAfter(downloadPath, 0);
        } else if (std::string(failure) == "close-candidate") {
            harness.fileSystem.setCloseFailure(secondaryPath);
        } else if (std::string(failure) == "read-candidate") {
            harness.fileSystem.setReadErrorAfter(secondaryPath, 0);
        }
        harness.send(finishCommand(streamHash));

        EXPECT_EQ(harness.fileSystem.contents(primaryPath), primaryBefore);
        EXPECT_EQ(harness.boot.snapshot().phase, UploadPhase::idle);
        EXPECT_FALSE(harness.esp.restarted);
        expectNoStagedResult(harness);
        if (std::string(failure) == "short-candidate" ||
            std::string(failure) == "close-candidate" ||
            std::string(failure) == "read-candidate") {
            const OtaResult* candidateFailure = lastResult(harness, "ERROR");
            ASSERT_NE(candidateFailure, nullptr);
            EXPECT_EQ(candidateFailure->reason, "candidate-write-failed");
            EXPECT_EQ(candidateFailure->hash, streamHash);
        }
    }
}

TEST(DeviceConfigOtaTest, StorageFaultsLatchUploadsUntilControllerIsRecreated) {
    const std::vector<std::pair<const char*, StorageFault>> failures = {
        {"envelope-open", StorageFault::envelopeOpen},
        {"envelope-write", StorageFault::envelopeWrite},
        {"envelope-close", StorageFault::envelopeClose},
        {"envelope-read-open", StorageFault::envelopeReadOpen},
        {"envelope-read", StorageFault::envelopeRead},
        {"candidate-write", StorageFault::candidateWrite},
        {"candidate-readback", StorageFault::candidateReadback},
        {"cleanup", StorageFault::cleanup},
        {"journal", StorageFault::journal}};

    for (const auto& [name, fault] : failures) {
        SCOPED_TRACE(name);
        OtaHarness harness;
        const std::string envelope = makeEnvelope(makeConfig(12));
        const std::vector<uint8_t> bytes = bytesOf(envelope);
        const std::string failedHash = sha256Hex(envelope);

        if (fault == StorageFault::envelopeOpen) {
            setStorageFault(harness, fault);
            harness.send(
                beginCommand(failedHash, static_cast<uint32_t>(bytes.size())));
        } else {
            harness.send(
                beginCommand(failedHash, static_cast<uint32_t>(bytes.size())));
            if (fault == StorageFault::envelopeWrite) {
                setStorageFault(harness, fault);
            }
            harness.send(chunkCommand(failedHash, 0, bytes));
            if (fault != StorageFault::envelopeWrite) {
                setStorageFault(harness, fault);
                harness.send(finishCommand(failedHash));
            }
        }

        EXPECT_GT(resultCount(harness, "ERROR"), 0u);
        expectNoStagedResult(harness);
        ASSERT_TRUE(harness.journal.state().has_value());
        const auto minimumAfterFault =
            harness.journal.state()->minimumAcceptedConfigVersion;
        ASSERT_TRUE(minimumAfterFault.has_value());
        const std::size_t verifierBeginsAfterFault =
            harness.verifier->beginCalls;

        setStorageFault(harness, fault, false);
        const std::string retryEnvelope =
            makeEnvelope(makeConfig(*minimumAfterFault));
        harness.sendEnvelope(retryEnvelope);

        EXPECT_EQ(harness.verifier->beginCalls, verifierBeginsAfterFault);
        expectNoStagedResult(harness);
        ASSERT_TRUE(harness.journal.state().has_value());
        EXPECT_EQ(
            harness.journal.state()->minimumAcceptedConfigVersion,
            minimumAfterFault);
    }
}

TEST(
    DeviceConfigOtaTest,
    AuthenticationAndDocumentFailuresRemainRetryableWhenStorageIsHealthy) {
    for (const char* failure : {"authentication", "document-validation"}) {
        SCOPED_TRACE(failure);
        OtaHarness harness;
        if (std::string(failure) == "authentication") {
            harness.verifier->finishResult = {
                SignatureStatus::invalid, SignatureError::invalidSignature};
        }

        const std::string invalidEnvelope =
            std::string(failure) == "authentication"
                ? makeEnvelope(makeConfig(12))
                : makeEnvelope(makeConfig(12), "AA:BB:CC:DD:EE:00");
        harness.sendEnvelope(invalidEnvelope);

        expectNoStagedResult(harness);
        ASSERT_TRUE(harness.journal.state().has_value());
        ASSERT_TRUE(
            harness.journal.state()->minimumAcceptedConfigVersion.has_value());
        EXPECT_EQ(
            harness.journal.state()->minimumAcceptedConfigVersion,
            std::optional<uint32_t>{12});

        harness.verifier->finishResult = {
            SignatureStatus::valid, SignatureError::none};
        harness.sendEnvelope(makeEnvelope(makeConfig(12)), streamHash);

        EXPECT_EQ(resultCount(harness, "STAGED"), 1u);
        ASSERT_TRUE(harness.journal.state().has_value());
        EXPECT_EQ(
            harness.journal.state()->minimumAcceptedConfigVersion,
            std::optional<uint32_t>{13});
    }
}

TEST(DeviceConfigOtaTest, JournalMirrorFailureDoesNotAcknowledgeStage) {
    OtaHarness harness;
    harness.fileSystem.setCloseFailure(backupPath);
    harness.sendEnvelope(makeEnvelope(makeConfig(12)));

    expectNoStagedResult(harness);
    EXPECT_FALSE(harness.boot.snapshot().updatesAllowed);
    EXPECT_FALSE(harness.esp.restarted);
    EXPECT_FALSE(harness.fileSystem.contents(downloadPath).has_value());
    EXPECT_EQ(resultCount(harness, "ERROR"), 1u);
}

TEST(DeviceConfigOtaTest, ReplayedFailedTrialVersionIsRejected) {
    const std::string failedCandidate = makeConfig(12);
    OtaHarness harness(true, UploadPhase::started, failedCandidate);
    EXPECT_FALSE(harness.selection.trial);
    EXPECT_EQ(harness.boot.snapshot().phase, UploadPhase::idle);
    EXPECT_EQ(harness.boot.snapshot().configVersion, 11u);
    EXPECT_EQ(
        harness.boot.snapshot().minimumAcceptedConfigVersion,
        std::optional<uint32_t>{13});
    ASSERT_TRUE(harness.boot.snapshot().lastOutcome.has_value());
    EXPECT_EQ(harness.boot.snapshot().lastOutcome->status, "ROLLED_BACK");
    const std::optional<std::string> failedCandidateBefore =
        harness.fileSystem.contents(secondaryPath);

    harness.sendEnvelope(makeEnvelope(failedCandidate));

    EXPECT_EQ(
        harness.fileSystem.contents(secondaryPath), failedCandidateBefore);
    EXPECT_FALSE(harness.fileSystem.contents(downloadPath).has_value());
    EXPECT_EQ(harness.boot.snapshot().phase, UploadPhase::idle);
    EXPECT_EQ(harness.boot.snapshot().configVersion, 11u);
    EXPECT_EQ(
        harness.boot.snapshot().minimumAcceptedConfigVersion,
        std::optional<uint32_t>{13});
    expectNoStagedResult(harness);
}

TEST(DeviceConfigOtaTest, CompletionDoesNotRestageOrRestartFromExecute) {
    OtaHarness harness;
    const std::string envelope = makeEnvelope(makeConfig(12));
    const std::vector<uint8_t> bytes = bytesOf(envelope);
    harness.send(beginCommand(streamHash, static_cast<uint32_t>(bytes.size())));
    harness.send(chunkCommand(streamHash, 0, bytes));

    harness.ota->execute(finishCommand(streamHash), false);
    EXPECT_FALSE(harness.esp.restarted);
    EXPECT_EQ(harness.boot.snapshot().phase, UploadPhase::idle);
    harness.ota->update();
    EXPECT_EQ(harness.boot.snapshot().phase, UploadPhase::pending);
    EXPECT_FALSE(harness.esp.restarted);
    ASSERT_TRUE(harness.journal.state().has_value());
    const uint32_t stagedSequence = harness.journal.state()->sequence;
    EXPECT_EQ(resultCount(harness, "STAGED"), 1u);

    harness.ota->update();
    EXPECT_TRUE(harness.esp.restarted);
    harness.ota->execute(finishCommand(streamHash), false);
    harness.ota->update();

    ASSERT_TRUE(harness.journal.state().has_value());
    EXPECT_EQ(harness.journal.state()->sequence, stagedSequence);
    EXPECT_EQ(resultCount(harness, "STAGED"), 1u);
}
