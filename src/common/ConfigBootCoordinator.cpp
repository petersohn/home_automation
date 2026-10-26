#include "ConfigBootCoordinator.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "../tools/JsonParsing.hpp"
#include "ArduinoJson.hpp"
#include "OtaDocument.hpp"
#include "UploadStatus.hpp"

namespace {

constexpr std::size_t maxConfigBytes = 32768;
constexpr std::size_t maxReasonBytes = 64;
constexpr const char* downloadPath = "/device_config.download.json";

bool removeAbandonedDownload(OtaFileSystem& fileSystem) {
    const OtaFilePresence presence = fileSystem.inspect(downloadPath);
    if (presence == OtaFilePresence::missing) {
        return true;
    }
    return presence == OtaFilePresence::present &&
           fileSystem.remove(downloadPath);
}

class BoundedConfigInput {
public:
    explicit BoundedConfigInput(OtaFile& file) : file(file) {}

    int read() {
        const int value = this->file.read();
        if (value == -1) {
            return value;
        }
        if (value < 0 || value > 255) {
            this->ioError = true;
            return -2;
        }
        if (this->bytesRead == maxConfigBytes) {
            this->tooLarge = true;
            return -2;
        }
        ++this->bytesRead;
        return value;
    }

    bool failed() const { return this->ioError || this->file.failed(); }
    bool exceededLimit() const { return this->tooLarge; }

private:
    OtaFile& file;
    std::size_t bytesRead = 0;
    bool ioError = false;
    bool tooLarge = false;
};

struct LoadedConfig {
    std::unique_ptr<ArduinoJson::DynamicJsonBuffer> buffer;
    ArduinoJson::JsonObject* root = nullptr;
    std::string reason;
    bool valid = false;
};

LoadedConfig readConfig(OtaFileSystem& fileSystem, ConfigSlot slot) {
    LoadedConfig result;
    const char* const path = configSlotPath(slot);
    if (path == nullptr) {
        result.reason = "invalid-slot";
        return result;
    }

    const OtaFilePresence presence = fileSystem.inspect(path);
    if (presence == OtaFilePresence::missing) {
        result.reason = "config-missing";
        return result;
    }
    if (presence == OtaFilePresence::error) {
        result.reason = "config-io-error";
        return result;
    }

    std::unique_ptr<OtaFile> file = fileSystem.open(path, OtaFileMode::read);
    if (file == nullptr) {
        result.reason = "config-io-error";
        return result;
    }

    result.buffer = std::make_unique<ArduinoJson::DynamicJsonBuffer>(1024);
    BoundedConfigInput input(*file);
    const ArduinoJson::JsonVariant parsed = tools::parseFullJson(
        input, *result.buffer, ARDUINOJSON_DEFAULT_NESTING_LIMIT);
    bool ioError = input.failed() || file->failed();
    const bool closeSucceeded = file->close();
    ioError = ioError || file->failed();
    if (ioError || !closeSucceeded) {
        result.reason = "config-io-error";
        return result;
    }
    if (input.exceededLimit()) {
        result.reason = "config-too-large";
        return result;
    }
    if (!parsed.success() || !parsed.is<ArduinoJson::JsonObject>()) {
        result.reason = "invalid-config-json";
        return result;
    }

    result.root = &parsed.as<ArduinoJson::JsonObject>();
    result.valid = true;
    return result;
}

bool isValidHash(const std::string& hash) {
    if (hash.size() != 64) {
        return false;
    }
    for (char value : hash) {
        if (!((value >= '0' && value <= '9') ||
              (value >= 'a' && value <= 'f'))) {
            return false;
        }
    }
    return true;
}

std::string boundedReason(const char* reason) {
    std::string result = reason == nullptr ? "rollback" : reason;
    if (result.empty()) {
        result = "rollback";
    }
    if (result.size() > maxReasonBytes) {
        result.resize(maxReasonBytes);
    }
    for (char& value : result) {
        if (static_cast<unsigned char>(value) < 0x20) {
            value = '-';
        }
    }
    return result;
}

OtaResult lifecycleResult(
    const UploadStatus& state, const char* status, const std::string& reason) {
    OtaResult result;
    result.status = status;
    result.transferred = state.trialBytes;
    result.hash = state.trialHash;
    result.version = state.trialVersion;
    result.reason = reason;
    return result;
}

void clearTrial(UploadStatus& state) {
    state.phase = UploadPhase::idle;
    state.trialSlot.reset();
    state.trialVersion.reset();
    state.trialHash.clear();
    state.trialBytes = 0;
}

}  // namespace

ConfigBootCoordinator::ConfigBootCoordinator(
    OtaFileSystem& fileSystem, UploadJournal& journal)
    : fileSystem(fileSystem), journal(journal) {}

ConfigBootSelection ConfigBootCoordinator::selectForBoot() {
    this->bootSelected = true;
    this->liveTrial = false;
    this->activeConfigAllowsOta = false;
    this->otaAllowed = false;
    this->runningSlot = ConfigSlot::primary;

    const JournalRecovery recovery = this->journal.recover();
    const bool downloadCleanupSucceeded =
        removeAbandonedDownload(this->fileSystem);
    if (!downloadCleanupSucceeded && recovery.mode != JournalMode::fresh) {
        const ConfigSlot fallback = recovery.state.has_value()
                                        ? recovery.state->activeSlot
                                        : ConfigSlot::primary;
        this->runningSlot = fallback;
        return {fallback, false, false, "download-cleanup-failed"};
    }

    if (recovery.mode == JournalMode::fresh) {
        LoadedConfig config = readConfig(this->fileSystem, ConfigSlot::primary);
        if (!config.valid) {
            return {ConfigSlot::primary, false, false, config.reason};
        }

        OtaConfigPolicy versionPolicy =
            inspectOtaConfig(*config.root, false, true);
        if (!versionPolicy.valid) {
            return {ConfigSlot::primary, false, false, versionPolicy.reason};
        }

        UploadStatus initial;
        initial.activeSlot = ConfigSlot::primary;
        initial.activeVersion = versionPolicy.version;
        initial.minimumAcceptedConfigVersion =
            initialMinimum(versionPolicy.version);
        if (!downloadCleanupSucceeded) {
            this->runningSlot = ConfigSlot::primary;
            return {
                ConfigSlot::primary, false, false, "download-cleanup-failed"};
        }
        if (!this->journal.store(initial)) {
            return {ConfigSlot::primary, false, false, "status-write-failed"};
        }

        const OtaConfigPolicy activePolicy =
            inspectOtaConfig(*config.root, true, true);
        this->activeConfigAllowsOta = activePolicy.valid;
        this->runningSlot = ConfigSlot::primary;
        this->otaAllowed =
            downloadCleanupSucceeded && this->activeConfigAllowsOta &&
            this->journal.writable() &&
            this->journal.state()->minimumAcceptedConfigVersion.has_value();
        return {
            ConfigSlot::primary, false, this->otaAllowed,
            !downloadCleanupSucceeded ? std::string("download-cleanup-failed")
            : activePolicy.valid      ? std::string()
                                      : activePolicy.reason};
    }

    if (recovery.mode != JournalMode::recovered ||
        !recovery.state.has_value()) {
        const ConfigSlot fallback = recovery.state.has_value()
                                        ? recovery.state->activeSlot
                                        : ConfigSlot::primary;
        const std::string reason = recovery.mode == JournalMode::ioError
                                       ? "status-io-error"
                                       : "status-corrupt";
        this->runningSlot = fallback;
        return {fallback, false, false, reason};
    }

    if (!this->journal.writable() && !this->journal.mirror()) {
        const UploadStatus& state = this->journal.state().has_value()
                                        ? *this->journal.state()
                                        : *recovery.state;
        this->runningSlot = state.activeSlot;
        return {state.activeSlot, false, false, "status-repair-failed"};
    }

    if (!this->journal.state().has_value()) {
        this->runningSlot = ConfigSlot::primary;
        return {ConfigSlot::primary, false, false, "status-unavailable"};
    }

    UploadStatus state = *this->journal.state();
    this->runningSlot = state.activeSlot;

    if (state.phase == UploadPhase::started) {
        (void)this->rollback("interrupted-trial");
        const auto& rolledBack = this->journal.state();
        if (rolledBack.has_value()) {
            state = *rolledBack;
        }
    }

    LoadedConfig confirmed = readConfig(this->fileSystem, state.activeSlot);
    OtaConfigPolicy confirmedVersion;
    OtaConfigPolicy confirmedPolicy;
    bool confirmedMatches = false;
    if (confirmed.valid) {
        confirmedVersion = inspectOtaConfig(*confirmed.root, false, true);
        confirmedMatches = confirmedVersion.valid &&
                           confirmedVersion.version == state.activeVersion;
        confirmedPolicy = inspectOtaConfig(*confirmed.root, true, true);
    }
    this->activeConfigAllowsOta = confirmedMatches && confirmedPolicy.valid;

    if (state.phase == UploadPhase::idle) {
        this->runningSlot = state.activeSlot;
        this->otaAllowed = this->activeConfigAllowsOta &&
                           this->journal.writable() &&
                           state.minimumAcceptedConfigVersion.has_value();
        std::string reason;
        if (!confirmed.valid) {
            reason = confirmed.reason;
        } else if (!confirmedMatches) {
            reason = confirmedVersion.valid ? "active-version-mismatch"
                                            : confirmedVersion.reason;
        } else if (!confirmedPolicy.valid) {
            reason = confirmedPolicy.reason;
        }
        return {state.activeSlot, false, this->otaAllowed, reason};
    }

    if (state.phase != UploadPhase::pending || !state.trialSlot.has_value() ||
        !state.trialVersion.has_value()) {
        this->runningSlot = state.activeSlot;
        return {state.activeSlot, false, false, "invalid-trial-state"};
    }

    if (!confirmed.valid || !confirmedMatches) {
        const char* const reason = !confirmed.valid ? confirmed.reason.c_str()
                                                    : "active-version-mismatch";
        const bool rollbackSucceeded = this->rollback(reason);
        const auto& rolledBack = this->journal.state();
        if (rolledBack.has_value()) {
            state = *rolledBack;
        }
        this->runningSlot = state.activeSlot;
        this->otaAllowed = rollbackSucceeded && this->activeConfigAllowsOta &&
                           this->journal.writable() &&
                           state.minimumAcceptedConfigVersion.has_value();
        return {state.activeSlot, false, this->otaAllowed, reason};
    }

    UploadStatus started = state;
    started.phase = UploadPhase::started;
    if (!this->journal.store(started)) {
        this->runningSlot = state.activeSlot;
        this->otaAllowed = false;
        return {
            state.activeSlot, false, false, "trial-start-persistence-failed"};
    }

    state = *this->journal.state();
    LoadedConfig candidate = readConfig(this->fileSystem, *state.trialSlot);
    OtaConfigPolicy candidatePolicy;
    if (candidate.valid) {
        candidatePolicy = inspectOtaConfig(*candidate.root, true, false);
    } else {
        candidatePolicy.reason = candidate.reason;
    }
    const bool validCandidate =
        candidatePolicy.valid && state.trialVersion.has_value() &&
        candidatePolicy.version == *state.trialVersion &&
        candidatePolicy.version > state.activeVersion &&
        (!state.minimumAcceptedConfigVersion.has_value() ||
         candidatePolicy.version < *state.minimumAcceptedConfigVersion);
    if (!validCandidate) {
        const std::string reason = candidatePolicy.reason.empty()
                                       ? "candidate-version-mismatch"
                                       : candidatePolicy.reason;
        const bool rollbackSucceeded = this->rollback(reason.c_str());
        const auto& rolledBack = this->journal.state();
        if (rolledBack.has_value()) {
            state = *rolledBack;
        }
        this->runningSlot = state.activeSlot;
        this->otaAllowed = rollbackSucceeded && this->activeConfigAllowsOta &&
                           this->journal.writable() &&
                           state.minimumAcceptedConfigVersion.has_value();
        return {state.activeSlot, false, this->otaAllowed, reason};
    }

    this->liveTrial = true;
    this->runningSlot = *state.trialSlot;
    this->otaAllowed = false;
    return {this->runningSlot, true, false, "trial"};
}

bool ConfigBootCoordinator::stage(
    ConfigSlot slot, uint32_t version, const std::string& hash,
    uint32_t bytes) {
    const auto& current = this->journal.state();
    if (!this->bootSelected || !this->otaAllowed || !this->journal.writable() ||
        !current.has_value() || current->phase != UploadPhase::idle ||
        slot != this->inactiveSlot() || !isValidHash(hash) ||
        !current->minimumAcceptedConfigVersion.has_value() ||
        version < *current->minimumAcceptedConfigVersion ||
        version <= current->activeVersion) {
        return false;
    }

    UploadStatus pending = *current;
    pending.phase = UploadPhase::pending;
    pending.trialSlot = slot;
    pending.trialVersion = version;
    pending.trialHash = hash;
    pending.trialBytes = bytes;
    if (!advanceMinimum(pending, version)) {
        return false;
    }
    pending.lastOutcome = lifecycleResult(pending, "STAGED", std::string());
    if (!this->journal.store(pending)) {
        this->otaAllowed = false;
        return false;
    }

    this->liveTrial = false;
    this->runningSlot = pending.activeSlot;
    this->otaAllowed = false;
    return true;
}

bool ConfigBootCoordinator::confirm() {
    const auto& current = this->journal.state();
    if (!this->liveTrial || !current.has_value() ||
        current->phase != UploadPhase::started ||
        !current->trialSlot.has_value() || !current->trialVersion.has_value()) {
        return false;
    }

    UploadStatus confirmed = *current;
    confirmed.activeSlot = *confirmed.trialSlot;
    confirmed.activeVersion = *confirmed.trialVersion;
    confirmed.lastOutcome = lifecycleResult(confirmed, "CONFIRMED", "");
    clearTrial(confirmed);
    if (!this->journal.store(confirmed)) {
        this->otaAllowed = false;
        const auto& updated = this->journal.state();
        if (updated.has_value() && updated->phase == UploadPhase::idle) {
            this->liveTrial = false;
            this->runningSlot = updated->activeSlot;
        }
        return false;
    }

    this->liveTrial = false;
    this->runningSlot = confirmed.activeSlot;
    this->activeConfigAllowsOta = true;
    this->otaAllowed = this->journal.writable() &&
                       confirmed.minimumAcceptedConfigVersion.has_value();
    return true;
}

bool ConfigBootCoordinator::rollback(const char* reason) {
    const auto& current = this->journal.state();
    if (!current.has_value() || current->phase == UploadPhase::idle ||
        !current->trialSlot.has_value() || !current->trialVersion.has_value()) {
        return false;
    }

    UploadStatus rolledBack = *current;
    rolledBack.lastOutcome =
        lifecycleResult(rolledBack, "ROLLED_BACK", boundedReason(reason));
    clearTrial(rolledBack);
    const bool stored = this->journal.store(rolledBack);
    this->liveTrial = false;
    this->runningSlot = rolledBack.activeSlot;
    this->otaAllowed = stored && this->activeConfigAllowsOta &&
                       this->journal.writable() &&
                       rolledBack.minimumAcceptedConfigVersion.has_value();
    return stored;
}

OtaStatusSnapshot ConfigBootCoordinator::snapshot() const {
    OtaStatusSnapshot result;
    const auto& current = this->journal.state();
    if (!current.has_value()) {
        if (this->bootSelected && this->journal.writable()) {
            LoadedConfig config =
                readConfig(this->fileSystem, this->runningSlot);
            if (config.valid) {
                const OtaConfigPolicy versionPolicy =
                    inspectOtaConfig(*config.root, false, true);
                if (versionPolicy.valid) {
                    result.configVersion = versionPolicy.version;
                }
            }
        }
        return result;
    }

    result.configVersion = this->liveTrial && current->trialVersion.has_value()
                               ? *current->trialVersion
                               : current->activeVersion;
    result.minimumAcceptedConfigVersion = current->minimumAcceptedConfigVersion;
    result.phase = current->phase;
    if (this->journal.writable()) {
        result.lastOutcome = current->lastOutcome;
    }
    result.updatesAllowed = this->otaAllowed && this->journal.writable() &&
                            current->phase == UploadPhase::idle &&
                            current->minimumAcceptedConfigVersion.has_value();
    return result;
}

bool ConfigBootCoordinator::trialRunning() const {
    return this->liveTrial;
}

ConfigSlot ConfigBootCoordinator::inactiveSlot() const {
    const auto& current = this->journal.state();
    const ConfigSlot active =
        current.has_value() ? current->activeSlot : ConfigSlot::primary;
    return active == ConfigSlot::primary ? ConfigSlot::secondary
                                         : ConfigSlot::primary;
}
