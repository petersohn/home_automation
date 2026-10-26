#ifndef COMMON_CONFIG_BOOT_COORDINATOR_HPP
#define COMMON_CONFIG_BOOT_COORDINATOR_HPP

#include <cstdint>
#include <string>

#include "OtaFile.hpp"
#include "OtaTypes.hpp"
#include "UploadJournal.hpp"

struct ConfigBootSelection {
    ConfigSlot slot = ConfigSlot::primary;
    bool trial = false;
    bool otaAllowed = false;
    std::string reason;
};

class ConfigBootCoordinator {
public:
    ConfigBootCoordinator(OtaFileSystem& fileSystem, UploadJournal& journal);

    ConfigBootSelection selectForBoot();
    bool stage(
        ConfigSlot slot, uint32_t version, const std::string& hash,
        uint32_t bytes);
    bool confirm();
    bool rollback(const char* reason);
    OtaStatusSnapshot snapshot() const;
    bool trialRunning() const;
    ConfigSlot inactiveSlot() const;

private:
    OtaFileSystem& fileSystem;
    UploadJournal& journal;
    bool bootSelected = false;
    bool liveTrial = false;
    bool activeConfigAllowsOta = false;
    bool otaAllowed = false;
    ConfigSlot runningSlot = ConfigSlot::primary;
};

#endif  // COMMON_CONFIG_BOOT_COORDINATOR_HPP
