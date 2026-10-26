#ifndef COMMON_OTA_TYPES_HPP
#define COMMON_OTA_TYPES_HPP

#include <cstdint>
#include <optional>
#include <string>

enum class ConfigSlot : uint8_t { primary = 1, secondary = 2 };

enum class UploadPhase { idle, pending, started };

struct OtaResult {
    std::string status;
    uint32_t transferred = 0;
    std::string hash;
    std::optional<uint32_t> version;
    std::string reason;
};

struct UploadStatus {
    uint32_t sequence = 0;
    ConfigSlot activeSlot = ConfigSlot::primary;
    uint32_t activeVersion = 0;
    UploadPhase phase = UploadPhase::idle;
    std::optional<ConfigSlot> trialSlot;
    std::optional<uint32_t> trialVersion;
    std::string trialHash;
    uint32_t trialBytes = 0;
    std::optional<uint32_t> minimumAcceptedConfigVersion = 1;
    std::optional<OtaResult> lastOutcome;
};

struct OtaStatusSnapshot {
    uint32_t configVersion = 0;
    std::optional<uint32_t> minimumAcceptedConfigVersion = 1;
    UploadPhase phase = UploadPhase::idle;
    std::optional<OtaResult> lastOutcome;
    bool updatesAllowed = false;
};

const char* configSlotPath(ConfigSlot slot);

#endif  // COMMON_OTA_TYPES_HPP
