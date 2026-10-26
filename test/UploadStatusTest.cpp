#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "common/OtaTypes.hpp"
#include "common/UploadStatus.hpp"

namespace {

const std::string knownIdleRecord =
    R"({"format":1,"sequence":0,"activeSlot":1,"activeVersion":0,"phase":"idle","trialSlot":null,"trialVersion":null,"trialHash":"","trialBytes":0,"minimumAcceptedConfigVersion":1,"lastOutcome":null,"checksum":"a2055894"})";

const std::string knownIdlePayload =
    R"({"format":1,"sequence":0,"activeSlot":1,"activeVersion":0,"phase":"idle","trialSlot":null,"trialVersion":null,"trialHash":"","trialBytes":0,"minimumAcceptedConfigVersion":1,"lastOutcome":null})";

std::string makeRecord(const std::string& payload, const char* checksum) {
    return payload.substr(0, payload.size() - 1) + ",\"checksum\":\"" +
           checksum + "\"}";
}

std::string replaceFieldValue(
    const std::string& payload, const std::string& field,
    const std::string& value) {
    const std::string marker = "\"" + field + "\":";
    const size_t valueStart = payload.find(marker);
    EXPECT_NE(valueStart, std::string::npos);
    if (valueStart == std::string::npos) {
        return std::string();
    }
    const size_t start = valueStart + marker.size();
    const size_t end = payload.find_first_of(",}", start);
    EXPECT_NE(end, std::string::npos);
    if (end == std::string::npos) {
        return std::string();
    }
    return payload.substr(0, start) + value + payload.substr(end);
}

std::string makePendingPayload(const std::string& hash) {
    return R"({"format":1,"sequence":7,"activeSlot":1,"activeVersion":10,"phase":"pending","trialSlot":2,"trialVersion":12,"trialHash":")" +
           hash +
           R"(","trialBytes":0,"minimumAcceptedConfigVersion":13,"lastOutcome":null})";
}

std::string makeStartedPayloadWithOutcome() {
    const std::string hash(64, 'a');
    return R"({"format":1,"sequence":8,"activeSlot":1,"activeVersion":10,"phase":"started","trialSlot":2,"trialVersion":12,"trialHash":")" +
           hash +
           R"(","trialBytes":4096,"minimumAcceptedConfigVersion":13,"lastOutcome":{"status":"STAGED","transferred":4096,"hash":")" +
           hash + R"(","version":12,"reason":""}})";
}

UploadStatus makePendingStatus() {
    UploadStatus state;
    state.sequence = 7;
    state.activeSlot = ConfigSlot::primary;
    state.activeVersion = 10;
    state.phase = UploadPhase::pending;
    state.trialSlot = ConfigSlot::secondary;
    state.trialVersion = 12;
    state.trialHash = std::string(64, 'a');
    state.trialBytes = 0;
    state.minimumAcceptedConfigVersion = 13;
    return state;
}

void expectStatusEquals(
    const UploadStatus& actual, const UploadStatus& expected) {
    EXPECT_EQ(actual.sequence, expected.sequence);
    EXPECT_EQ(actual.activeSlot, expected.activeSlot);
    EXPECT_EQ(actual.activeVersion, expected.activeVersion);
    EXPECT_EQ(actual.phase, expected.phase);
    EXPECT_EQ(actual.trialSlot, expected.trialSlot);
    EXPECT_EQ(actual.trialVersion, expected.trialVersion);
    EXPECT_EQ(actual.trialHash, expected.trialHash);
    EXPECT_EQ(actual.trialBytes, expected.trialBytes);
    EXPECT_EQ(
        actual.minimumAcceptedConfigVersion,
        expected.minimumAcceptedConfigVersion);
    ASSERT_EQ(actual.lastOutcome.has_value(), expected.lastOutcome.has_value());
    if (expected.lastOutcome.has_value()) {
        EXPECT_EQ(actual.lastOutcome->status, expected.lastOutcome->status);
        EXPECT_EQ(
            actual.lastOutcome->transferred, expected.lastOutcome->transferred);
        EXPECT_EQ(actual.lastOutcome->hash, expected.lastOutcome->hash);
        EXPECT_EQ(actual.lastOutcome->version, expected.lastOutcome->version);
        EXPECT_EQ(actual.lastOutcome->reason, expected.lastOutcome->reason);
    }
}

void expectDecodeFailureUnchanged(const std::string& input) {
    UploadStatus output = makePendingStatus();
    output.lastOutcome =
        OtaResult{"STAGED", 128, std::string(64, 'a'), 12u, ""};
    const UploadStatus before = output;

    EXPECT_FALSE(decodeUploadStatus(input, output));
    expectStatusEquals(output, before);
}

}  // namespace

TEST(UploadStatusTest, UsesKnownFormatOneChecksumFixture) {
    UploadStatus decoded;
    ASSERT_TRUE(decodeUploadStatus(knownIdleRecord, decoded));

    UploadStatus expected;
    expectStatusEquals(decoded, expected);
    EXPECT_EQ(encodeUploadStatus(expected), knownIdleRecord);
}

TEST(UploadStatusTest, RoundTripEveryPhase) {
    std::vector<UploadStatus> states;
    states.emplace_back();
    states.push_back(makePendingStatus());

    UploadStatus started = makePendingStatus();
    started.sequence = 8;
    started.phase = UploadPhase::started;
    started.trialBytes = 4096;
    started.lastOutcome =
        OtaResult{"STAGED", 4096, std::string(64, 'a'), 12u, ""};
    states.push_back(started);

    for (const UploadStatus& state : states) {
        SCOPED_TRACE(static_cast<int>(state.phase));
        const std::string encoded = encodeUploadStatus(state);
        ASSERT_FALSE(encoded.empty());

        UploadStatus decoded;
        ASSERT_TRUE(decodeUploadStatus(encoded, decoded));
        expectStatusEquals(decoded, state);
    }
}

TEST(UploadStatusTest, RejectsUnknownMissingAndWronglyTypedFields) {
    const std::string withUnknownField =
        knownIdlePayload.substr(0, knownIdlePayload.size() - 1) +
        R"(,"unexpected":true})";
    expectDecodeFailureUnchanged(makeRecord(withUnknownField, "a2055894"));

    const std::string missingSequence =
        R"({"format":1,"activeSlot":1,"activeVersion":0,"phase":"idle","trialSlot":null,"trialVersion":null,"trialHash":"","trialBytes":0,"minimumAcceptedConfigVersion":1,"lastOutcome":null})";
    expectDecodeFailureUnchanged(makeRecord(missingSequence, "a2055894"));

    expectDecodeFailureUnchanged(
        R"({"format":2,"sequence":0,"activeSlot":1,"activeVersion":0,"phase":"idle","trialSlot":null,"trialVersion":null,"trialHash":"","trialBytes":0,"minimumAcceptedConfigVersion":1,"lastOutcome":null,"checksum":"a2055894"})");
    expectDecodeFailureUnchanged(
        R"({"format":1,"sequence":0.5,"activeSlot":1,"activeVersion":0,"phase":"idle","trialSlot":null,"trialVersion":null,"trialHash":"","trialBytes":0,"minimumAcceptedConfigVersion":1,"lastOutcome":null,"checksum":"a2055894"})");
    expectDecodeFailureUnchanged(
        R"({"format":1,"sequence":0,"activeSlot":1,"activeVersion":0,"phase":"unknown","trialSlot":null,"trialVersion":null,"trialHash":"","trialBytes":0,"minimumAcceptedConfigVersion":1,"lastOutcome":null,"checksum":"a2055894"})");
}

TEST(UploadStatusTest, RejectsImpossibleSlotPhaseAndVersionRelationships) {
    const std::string hash(64, 'a');
    const std::vector<std::pair<std::string, std::string>> invalidRecords = {
        {R"({"format":1,"sequence":1,"activeSlot":1,"activeVersion":10,"phase":"pending","trialSlot":1,"trialVersion":12,"trialHash":")" +
             hash +
             R"(","trialBytes":0,"minimumAcceptedConfigVersion":13,"lastOutcome":null})",
         "20b9e121"},
        {R"({"format":1,"sequence":1,"activeSlot":1,"activeVersion":10,"phase":"pending","trialSlot":null,"trialVersion":12,"trialHash":")" +
             hash +
             R"(","trialBytes":0,"minimumAcceptedConfigVersion":13,"lastOutcome":null})",
         "425bb289"},
        {R"({"format":1,"sequence":1,"activeSlot":1,"activeVersion":10,"phase":"started","trialSlot":2,"trialVersion":12,"trialHash":"","trialBytes":0,"minimumAcceptedConfigVersion":13,"lastOutcome":null})",
         "5b14fbfb"},
        {R"({"format":1,"sequence":1,"activeSlot":1,"activeVersion":10,"phase":"pending","trialSlot":2,"trialVersion":10,"trialHash":")" +
             hash +
             R"(","trialBytes":0,"minimumAcceptedConfigVersion":13,"lastOutcome":null})",
         "ad25cab1"},
        {R"({"format":1,"sequence":1,"activeSlot":1,"activeVersion":10,"phase":"started","trialSlot":2,"trialVersion":13,"trialHash":")" +
             hash +
             R"(","trialBytes":0,"minimumAcceptedConfigVersion":13,"lastOutcome":null})",
         "8b691249"},
        {R"({"format":1,"sequence":1,"activeSlot":1,"activeVersion":10,"phase":"idle","trialSlot":null,"trialVersion":12,"trialHash":"","trialBytes":0,"minimumAcceptedConfigVersion":13,"lastOutcome":null})",
         "6980fa93"},
        {R"({"format":1,"sequence":1,"activeSlot":1,"activeVersion":0,"phase":"idle","trialSlot":null,"trialVersion":null,"trialHash":"","trialBytes":0,"minimumAcceptedConfigVersion":0,"lastOutcome":null})",
         "9804b142"},
        {R"({"format":1,"sequence":1,"activeSlot":1,"activeVersion":12,"phase":"idle","trialSlot":null,"trialVersion":null,"trialHash":"","trialBytes":0,"minimumAcceptedConfigVersion":12,"lastOutcome":null})",
         "4d9baf74"},
        {R"({"format":1,"sequence":1,"activeSlot":1,"activeVersion":10,"phase":"pending","trialSlot":2,"trialVersion":12,"trialHash":")" +
             std::string(64, 'g') +
             R"(","trialBytes":0,"minimumAcceptedConfigVersion":13,"lastOutcome":null})",
         "890c6d86"},
    };

    for (const auto& testCase : invalidRecords) {
        SCOPED_TRACE(testCase.second);
        expectDecodeFailureUnchanged(
            makeRecord(testCase.first, testCase.second.c_str()));
    }
}

TEST(UploadStatusTest, ChecksumDetectsMutationTruncationAndNonFullInput) {
    std::string mutated = knownIdleRecord;
    const size_t checksumPosition = mutated.find("a2055894");
    ASSERT_NE(checksumPosition, std::string::npos);
    mutated[checksumPosition] = 'b';
    expectDecodeFailureUnchanged(mutated);

    mutated = knownIdleRecord;
    const size_t activeVersionPosition = mutated.find("activeVersion\":0");
    ASSERT_NE(activeVersionPosition, std::string::npos);
    mutated.replace(
        activeVersionPosition, std::string("activeVersion\":0").size(),
        "activeVersion\":1");
    expectDecodeFailureUnchanged(mutated);
    expectDecodeFailureUnchanged(
        knownIdleRecord.substr(0, knownIdleRecord.size() - 1));
    expectDecodeFailureUnchanged(knownIdleRecord + " trailing");

    std::string withNul = knownIdleRecord;
    withNul.push_back('\0');
    withNul += "tail";
    expectDecodeFailureUnchanged(withNul);

    expectDecodeFailureUnchanged(knownIdleRecord + std::string(4097, ' '));
}

TEST(UploadStatusTest, AcceptsVersionJumpAndRejectsReplayWithoutMutation) {
    UploadStatus state;
    ASSERT_TRUE(advanceMinimum(state, 12));
    ASSERT_EQ(state.minimumAcceptedConfigVersion, std::optional<uint32_t>{13});

    const UploadStatus afterAccept = state;
    EXPECT_FALSE(advanceMinimum(state, 12));
    expectStatusEquals(state, afterAccept);
}

TEST(UploadStatusTest, RollbackDoesNotLowerMinimumBoundary) {
    UploadStatus state;
    state.activeVersion = 11;
    state.minimumAcceptedConfigVersion = 13;
    const UploadStatus beforeRollback = state;

    EXPECT_FALSE(advanceMinimum(state, 11));
    expectStatusEquals(state, beforeRollback);
}

TEST(UploadStatusTest, MaximumVersionExhaustsInsteadOfWrapping) {
    UploadStatus state;
    state.minimumAcceptedConfigVersion = UINT32_MAX;
    ASSERT_TRUE(advanceMinimum(state, UINT32_MAX));
    EXPECT_FALSE(state.minimumAcceptedConfigVersion.has_value());

    const UploadStatus exhausted = state;
    EXPECT_FALSE(advanceMinimum(state, UINT32_MAX));
    expectStatusEquals(state, exhausted);
}

TEST(UploadStatusTest, InitialMinimumHandlesZeroAndMaximum) {
    ASSERT_EQ(initialMinimum(0), std::optional<uint32_t>{1});
    EXPECT_FALSE(initialMinimum(UINT32_MAX).has_value());
}

TEST(UploadStatusTest, EncodesOtaResultWithFixedFieldsAndNullVersion) {
    const OtaResult result{"PROGRESS", 17, "", std::nullopt, "working"};
    EXPECT_EQ(
        encodeOtaResult(result),
        R"({"status":"PROGRESS","transferred":17,"hash":"","version":null,"reason":"working"})");
}

TEST(UploadStatusTest, RejectsControlBytesInReasonForBothEncoders) {
    const std::vector<std::string> reasons = {
        std::string("before\0after", 12), std::string("before\001after", 12),
        "line\nbreak"};

    for (const std::string& reason : reasons) {
        SCOPED_TRACE(reason.size());
        const OtaResult result{
            "STAGED", 128, std::string(64, 'a'), 12u, reason};
        EXPECT_TRUE(encodeOtaResult(result).empty());

        UploadStatus status = makePendingStatus();
        status.lastOutcome = result;
        EXPECT_TRUE(encodeUploadStatus(status).empty());
    }
}

TEST(UploadStatusTest, RoundTripsUint32BoundariesAndNullMinimum) {
    UploadStatus highBit;
    highBit.sequence = 0x80000000u;
    highBit.activeVersion = 0x80000000u;
    highBit.minimumAcceptedConfigVersion.reset();

    std::string encoded = encodeUploadStatus(highBit);
    ASSERT_FALSE(encoded.empty());
    EXPECT_NE(encoded.find(R"("sequence":2147483648)"), std::string::npos);
    EXPECT_NE(encoded.find(R"("activeVersion":2147483648)"), std::string::npos);
    EXPECT_NE(
        encoded.find(R"("minimumAcceptedConfigVersion":null)"),
        std::string::npos);

    UploadStatus decoded;
    ASSERT_TRUE(decodeUploadStatus(encoded, decoded));
    expectStatusEquals(decoded, highBit);

    UploadStatus maximum = makePendingStatus();
    maximum.sequence = UINT32_MAX;
    maximum.activeVersion = 0x80000000u;
    maximum.trialVersion = UINT32_MAX;
    maximum.trialBytes = UINT32_MAX;
    maximum.minimumAcceptedConfigVersion.reset();

    encoded = encodeUploadStatus(maximum);
    ASSERT_FALSE(encoded.empty());
    EXPECT_NE(encoded.find(R"("sequence":4294967295)"), std::string::npos);
    EXPECT_NE(encoded.find(R"("trialVersion":4294967295)"), std::string::npos);
    EXPECT_NE(encoded.find(R"("trialBytes":4294967295)"), std::string::npos);
    ASSERT_TRUE(decodeUploadStatus(encoded, decoded));
    expectStatusEquals(decoded, maximum);
}

TEST(UploadStatusTest, RejectsNonUint32TokensWithCanonicalChecksum) {
    const std::vector<std::pair<std::string, std::string>> invalidValues = {
        {"format", "true"},
        {"format", "\"1\""},
        {"format", "null"},
        {"sequence", "false"},
        {"sequence", "\"0\""},
        {"sequence", "null"},
        {"activeSlot", "true"},
        {"activeSlot", "\"1\""},
        {"activeSlot", "null"},
        {"activeVersion", "false"},
        {"activeVersion", "\"0\""},
        {"activeVersion", "null"},
        {"trialBytes", "false"},
        {"trialBytes", "\"0\""},
        {"trialBytes", "null"},
        {"minimumAcceptedConfigVersion", "true"},
        {"minimumAcceptedConfigVersion", "\"1\""},
        {"sequence", "4294967296"},
    };

    for (const auto& testCase : invalidValues) {
        SCOPED_TRACE(testCase.first + "=" + testCase.second);
        const std::string payload = replaceFieldValue(
            knownIdlePayload, testCase.first, testCase.second);
        expectDecodeFailureUnchanged(makeRecord(payload, "a2055894"));
    }

    const std::string outcomePayload = makeStartedPayloadWithOutcome();
    for (const auto& testCase :
         std::vector<std::pair<std::string, std::string>>{
             {"transferred", "\"4096\""},
             {"transferred", "true"},
             {"transferred", "null"},
             {"version", "\"12\""},
             {"version", "true"},
             {"version", "null"}}) {
        SCOPED_TRACE("lastOutcome." + testCase.first + "=" + testCase.second);
        const std::string payload =
            replaceFieldValue(outcomePayload, testCase.first, testCase.second);
        expectDecodeFailureUnchanged(makeRecord(payload, "c434f4e8"));
    }
}

TEST(UploadStatusTest, RequiresExactly64LowercaseHashCharacters) {
    const std::string validPayload = makePendingPayload(std::string(64, 'a'));
    UploadStatus decoded;
    ASSERT_TRUE(
        decodeUploadStatus(makeRecord(validPayload, "f0ea5d90"), decoded));
    expectStatusEquals(decoded, makePendingStatus());

    const std::vector<std::pair<size_t, const char*>> invalidHashes = {
        {63, "32c6e49a"}, {65, "10fd9837"}};
    for (const auto& testCase : invalidHashes) {
        SCOPED_TRACE(testCase.first);
        const std::string payload =
            makePendingPayload(std::string(testCase.first, 'a'));
        expectDecodeFailureUnchanged(makeRecord(payload, testCase.second));
    }
}

TEST(UploadStatusTest, EnforcesReasonLengthBoundaryForBothEncoders) {
    const std::string acceptedReason(64, 'r');
    const std::string expectedResult =
        R"({"status":"PROGRESS","transferred":0,"hash":"","version":null,"reason":")" +
        acceptedReason + "\"}";
    EXPECT_EQ(
        encodeOtaResult(
            OtaResult{"PROGRESS", 0, "", std::nullopt, acceptedReason}),
        expectedResult);
    EXPECT_TRUE(
        encodeOtaResult(
            OtaResult{"PROGRESS", 0, "", std::nullopt, acceptedReason + "r"})
            .empty());

    UploadStatus status = makePendingStatus();
    status.lastOutcome =
        OtaResult{"STAGED", 128, std::string(64, 'a'), 12u, acceptedReason};
    EXPECT_FALSE(encodeUploadStatus(status).empty());
    status.lastOutcome->reason += "r";
    EXPECT_TRUE(encodeUploadStatus(status).empty());
}

TEST(UploadStatusTest, EncodesFailAndErrorWireResults) {
    EXPECT_EQ(
        encodeOtaResult(
            OtaResult{"FAIL", 0x80000000u, "", std::nullopt, "rejected"}),
        R"({"status":"FAIL","transferred":2147483648,"hash":"","version":null,"reason":"rejected"})");
    EXPECT_EQ(
        encodeOtaResult(
            OtaResult{"ERROR", UINT32_MAX, "", UINT32_MAX, "storage"}),
        R"({"status":"ERROR","transferred":4294967295,"hash":"","version":4294967295,"reason":"storage"})");
}

TEST(UploadStatusTest, RejectsInvalidStateInsteadOfNormalizing) {
    UploadStatus invalid = makePendingStatus();
    invalid.trialSlot = ConfigSlot::primary;
    EXPECT_TRUE(encodeUploadStatus(invalid).empty());
}
