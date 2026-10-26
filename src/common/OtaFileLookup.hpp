#ifndef COMMON_OTA_FILE_LOOKUP_HPP
#define COMMON_OTA_FILE_LOOKUP_HPP

#include <functional>

#include "OtaFile.hpp"

inline OtaFilePresence classifyOtaStatResult(
    int result, int successCode, int notFoundCode) {
    if (result == successCode) {
        return OtaFilePresence::present;
    }
    if (result == notFoundCode) {
        return OtaFilePresence::missing;
    }
    return OtaFilePresence::error;
}

inline bool removeOtaFileIfPresent(
    OtaFilePresence presence, const std::function<bool()>& remove) {
    if (presence == OtaFilePresence::missing) {
        return true;
    }
    return presence == OtaFilePresence::present && remove && remove();
}

#endif  // COMMON_OTA_FILE_LOOKUP_HPP
