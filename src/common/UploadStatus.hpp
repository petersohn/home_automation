#ifndef COMMON_UPLOAD_STATUS_HPP
#define COMMON_UPLOAD_STATUS_HPP

#include <cstdint>
#include <optional>
#include <string>

#include "OtaTypes.hpp"

bool decodeUploadStatus(const std::string& input, UploadStatus& output);
std::string encodeUploadStatus(const UploadStatus& state);
bool advanceMinimum(UploadStatus& state, uint32_t acceptedVersion);
std::optional<uint32_t> initialMinimum(uint32_t activeVersion);
std::string encodeOtaResult(const OtaResult& result);

#endif  // COMMON_UPLOAD_STATUS_HPP
