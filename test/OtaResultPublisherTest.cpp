#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "common/OtaResultPublisher.hpp"

namespace {

const std::string validHash(64, 'a');

OtaResult lifecycleResult(const char* status, uint32_t version) {
    return OtaResult{status, 12, validHash, version, ""};
}

OtaResult ordinaryResult(std::string reason) {
    return OtaResult{"ERROR", 0, "", std::nullopt, std::move(reason)};
}

}  // namespace

TEST(OtaResultPublisherTest, FailedFirstSubmissionRemainsPendingForRetry) {
    OtaResultPublisher publisher;
    publisher.report(lifecycleResult("STAGED", 8));
    std::vector<std::string> submitted;

    publisher.flush([&submitted](const OtaResult& result) {
        submitted.push_back(result.status);
        return false;
    });
    EXPECT_EQ(submitted, std::vector<std::string>{"STAGED"});

    publisher.flush([&submitted](const OtaResult& result) {
        submitted.push_back(result.status);
        return true;
    });
    publisher.flush([&submitted](const OtaResult& result) {
        submitted.push_back(result.status);
        return true;
    });

    EXPECT_EQ(submitted, (std::vector<std::string>{"STAGED", "STAGED"}));
}

TEST(OtaResultPublisherTest, AcceptedSlotClearsWithoutClearingFailedPeer) {
    OtaResultPublisher publisher;
    publisher.report(lifecycleResult("STAGED", 8));
    publisher.report(ordinaryResult("rejected-command"));
    std::vector<std::string> submitted;

    publisher.flush([&submitted](const OtaResult& result) {
        submitted.push_back(result.status + ":" + result.reason);
        return result.status == "STAGED";
    });

    EXPECT_EQ(
        submitted,
        (std::vector<std::string>{"STAGED:", "ERROR:rejected-command"}));
    submitted.clear();
    publisher.flush([&submitted](const OtaResult& result) {
        submitted.push_back(result.status + ":" + result.reason);
        return true;
    });
    EXPECT_EQ(submitted, std::vector<std::string>{"ERROR:rejected-command"});

    submitted.clear();
    publisher.flush([&submitted](const OtaResult& result) {
        submitted.push_back(result.status);
        return true;
    });
    EXPECT_TRUE(submitted.empty());
}

TEST(OtaResultPublisherTest, RepeatedReportsCoalesceIntoTwoBoundedSlots) {
    OtaResultPublisher publisher;
    for (uint32_t version = 1; version <= 50; ++version) {
        publisher.report(lifecycleResult("CONFIRMED", version));
        publisher.report(ordinaryResult("failure-" + std::to_string(version)));
    }
    std::vector<std::string> rejected;

    publisher.flush([&rejected](const OtaResult& result) {
        rejected.push_back(result.status + ":" + result.reason);
        return false;
    });

    EXPECT_EQ(
        rejected, (std::vector<std::string>{"CONFIRMED:", "ERROR:failure-50"}));
}
