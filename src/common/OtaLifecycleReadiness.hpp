#ifndef COMMON_OTA_LIFECYCLE_READINESS_HPP
#define COMMON_OTA_LIFECYCLE_READINESS_HPP

struct OtaLifecycleReadiness {
    bool availabilityReady = false;
    bool canPublish = false;
    bool certificateValid = false;
    bool configurationPolicyValid = false;
    bool serviceSubscribed = false;
    bool commandSubscriptionReady = false;
};

inline bool isOtaLifecycleReady(const OtaLifecycleReadiness& readiness) {
    return readiness.availabilityReady && readiness.canPublish &&
           readiness.certificateValid && readiness.configurationPolicyValid &&
           readiness.serviceSubscribed && readiness.commandSubscriptionReady;
}

#endif  // COMMON_OTA_LIFECYCLE_READINESS_HPP
