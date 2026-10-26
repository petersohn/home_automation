#ifndef COMMON_OTA_RESULT_PUBLISHER_HPP
#define COMMON_OTA_RESULT_PUBLISHER_HPP

#include <functional>
#include <optional>

#include "OtaTypes.hpp"

class OtaResultPublisher {
public:
    void report(const OtaResult& result) {
        if (isLifecycleOutcome(result.status)) {
            this->lifecycleOutcome = result;
        } else {
            this->ordinaryResult = result;
        }
    }

    bool hasPendingLifecycle() const {
        return this->lifecycleOutcome.has_value();
    }

    void flush(const std::function<bool(const OtaResult&)>& submit) {
        if (!submit) {
            return;
        }
        if (this->lifecycleOutcome.has_value() &&
            submit(*this->lifecycleOutcome)) {
            this->lifecycleOutcome.reset();
        }
        if (this->ordinaryResult.has_value() && submit(*this->ordinaryResult)) {
            this->ordinaryResult.reset();
        }
    }

private:
    std::optional<OtaResult> lifecycleOutcome;
    std::optional<OtaResult> ordinaryResult;

    static bool isLifecycleOutcome(const std::string& status) {
        return status == "STAGED" || status == "CONFIRMED" ||
               status == "ROLLED_BACK";
    }
};

#endif  // COMMON_OTA_RESULT_PUBLISHER_HPP
