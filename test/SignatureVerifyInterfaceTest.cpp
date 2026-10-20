#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "InterfaceTestBase.hpp"
#include "common/MqttTransportState.hpp"
#include "common/SignatureVerifier.hpp"
#include "common/SignatureVerifyInterface.hpp"

namespace {

std::vector<uint8_t> copyBytes(SignatureBytes bytes) {
    std::vector<uint8_t> out;
    out.reserve(bytes.size);
    for (std::size_t index = 0; index < bytes.size; ++index) {
        out.push_back(bytes.data[index]);
    }
    return out;
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

std::vector<std::string> reply(
    const std::string& status, uint32_t count, const std::string& hash) {
    return {status, std::to_string(count), hash};
}

std::string beginCommand(const std::string& hash, uint32_t size) {
    return "{\"type\":\"begin\",\"hash\":\"" + hash +
           "\",\"size\":" + std::to_string(size) + "}";
}

std::string chunkCommand(
    const std::string& hash, uint32_t offset,
    const std::vector<uint8_t>& bytes) {
    return "{\"type\":\"chunk\",\"hash\":\"" + hash +
           "\",\"offset\":" + std::to_string(offset) + ",\"data\":\"" +
           base64Encode(bytes) + "\"}";
}

std::string finishCommand(const std::string& hash) {
    return "{\"type\":\"finish\",\"hash\":\"" + hash + "\",\"signature\":\"" +
           base64Encode(std::vector<uint8_t>(64, 0xA5)) + "\"}";
}

class FakeVerifier : public SignatureVerifier {
public:
    SignatureError beginError = SignatureError::none;
    SignatureError appendError = SignatureError::none;
    SignatureResult finishResult{SignatureStatus::valid, SignatureError::none};
    std::size_t beginCallCount = 0;
    std::size_t appendCallCount = 0;
    std::size_t finishCallCount = 0;
    std::size_t abortCallCount = 0;
    std::vector<std::vector<uint8_t>> receivedData;
    std::vector<std::vector<uint8_t>> receivedExpectedHashes;
    std::vector<std::vector<uint8_t>> receivedSignatures;

    SignatureError begin() override {
        ++this->beginCallCount;
        return this->beginError;
    }

    SignatureError append(SignatureBytes data) override {
        ++this->appendCallCount;
        this->receivedData.push_back(copyBytes(data));
        return this->appendError;
    }

    SignatureResult finish(
        SignatureBytes expectedHash, SignatureBytes signature) override {
        ++this->finishCallCount;
        this->receivedExpectedHashes.push_back(copyBytes(expectedHash));
        this->receivedSignatures.push_back(copyBytes(signature));
        return this->finishResult;
    }

    void abort() override { ++this->abortCallCount; }
};

class ForwardingVerifier : public SignatureVerifier {
public:
    explicit ForwardingVerifier(std::shared_ptr<FakeVerifier> target)
        : target(std::move(target)) {}

    SignatureError begin() override { return this->target->begin(); }

    SignatureError append(SignatureBytes data) override {
        return this->target->append(data);
    }

    SignatureResult finish(
        SignatureBytes expectedHash, SignatureBytes signature) override {
        return this->target->finish(expectedHash, signature);
    }

    void abort() override { this->target->abort(); }

private:
    std::shared_ptr<FakeVerifier> target;
};

class CapturingAction : public Action {
public:
    std::vector<std::vector<std::string>> firedValues;

    void fire(const InterfaceConfig& interface) override {
        this->firedValues.push_back(interface.storedValue);
    }

    void reset() override { this->firedValues.clear(); }
};

}  // namespace

class SignatureVerifyInterfaceTest : public InterfaceTestBase {
public:
    const std::string hash = std::string(64, 'a');
    const std::string otherHash = std::string(64, 'b');
    std::shared_ptr<CapturingAction> action =
        std::make_shared<CapturingAction>();
    std::shared_ptr<FakeVerifier> verifier = std::make_shared<FakeVerifier>();
    MqttTransportState transport{true, 0};

    SignatureVerifyInterfaceTest() {
        this->interface.name = "signature-verify";
        this->interface.actions.push_back(this->action);
        this->interface.interface = std::make_unique<SignatureVerifyInterface>(
            this->esp, std::make_unique<ForwardingVerifier>(this->verifier),
            [this]() { return this->transport; });
        this->interface.interface->start();
    }

    void execute(const std::string& command) {
        this->interface.interface->execute(command);
    }

    void update() { this->updateInterface(); }

    void start() { this->update(); }

    void send(const std::string& command) {
        this->execute(command);
        this->update();
    }

    void expectReply(
        const std::string& status, uint32_t count, const std::string& hash) {
        EXPECT_EQ(this->interface.storedValue, reply(status, count, hash));
    }
};

TEST_F(SignatureVerifyInterfaceTest, InitialStoredValueIsEmptyEvent) {
    EXPECT_EQ(this->interface.storedValue, std::vector<std::string>());
    this->start();
    EXPECT_EQ(this->interface.storedValue, std::vector<std::string>{""});
    EXPECT_EQ(this->action->firedValues.size(), 1u);
    this->update();
    this->update();
    EXPECT_EQ(this->action->firedValues.size(), 1u);
}

TEST_F(SignatureVerifyInterfaceTest, BeginEmitsCorrelatedProgress) {
    this->start();
    this->send(beginCommand(this->hash, 256));
    this->expectReply("PROGRESS", 0, this->hash);
    EXPECT_EQ(this->verifier->beginCallCount, 1u);
    EXPECT_EQ(this->verifier->appendCallCount, 0u);
    EXPECT_EQ(this->verifier->finishCallCount, 0u);
}

TEST_F(SignatureVerifyInterfaceTest, TimeoutDiscardsQueuedDuplicateBegin) {
    this->start();
    this->send(beginCommand(this->hash, 1));
    this->execute(beginCommand(this->hash, 1));
    this->esp.delay(30000);
    this->update();
    this->expectReply("ERROR timeout", 0, this->hash);
    const auto eventCount = this->action->firedValues.size();
    this->update();
    this->update();
    EXPECT_EQ(this->action->firedValues.size(), eventCount);
    EXPECT_EQ(this->verifier->beginCallCount, 1u);
    this->send(beginCommand(this->hash, 1));
    this->expectReply("PROGRESS", 0, this->hash);
    EXPECT_EQ(this->verifier->beginCallCount, 2u);
}

TEST_F(SignatureVerifyInterfaceTest, TimeoutDiscardsQueuedChunk) {
    this->start();
    this->send(beginCommand(this->hash, 1));
    this->execute(chunkCommand(this->hash, 0, {0x11}));
    this->esp.delay(30000);
    this->update();
    this->expectReply("ERROR timeout", 0, this->hash);
    const auto eventCount = this->action->firedValues.size();
    this->update();
    this->update();
    EXPECT_EQ(this->action->firedValues.size(), eventCount);
    EXPECT_EQ(this->verifier->appendCallCount, 0u);
}

TEST_F(
    SignatureVerifyInterfaceTest, AppendsChunkOnceAndReportsCumulativeBytes) {
    this->start();
    this->send(beginCommand(this->hash, 3));
    this->send(chunkCommand(this->hash, 0, {0x00, 0xFF}));
    this->expectReply("PROGRESS", 2, this->hash);
    ASSERT_EQ(this->verifier->receivedData.size(), 1u);
    EXPECT_EQ(
        this->verifier->receivedData[0], (std::vector<uint8_t>{0x00, 0xFF}));
}

TEST_F(
    SignatureVerifyInterfaceTest,
    DuplicateBeginAndChunkReackWithoutRefreshingTimeout) {
    this->start();
    this->send(beginCommand(this->hash, 2));
    this->send(chunkCommand(this->hash, 0, {0x11, 0x22}));
    this->esp.delay(15000);
    this->send(beginCommand(this->hash, 2));
    this->send(chunkCommand(this->hash, 0, {0x11, 0x22}));
    this->expectReply("PROGRESS", 2, this->hash);
    EXPECT_EQ(this->verifier->appendCallCount, 1u);
    this->esp.delay(15000);
    this->update();
    this->expectReply("ERROR timeout", 2, this->hash);
    EXPECT_EQ(this->verifier->appendCallCount, 1u);
}

TEST_F(SignatureVerifyInterfaceTest, FinishReportsStatusCountAndHash) {
    this->start();
    this->send(beginCommand(this->hash, 2));
    this->send(chunkCommand(this->hash, 0, {0x11, 0x22}));
    this->send(finishCommand(this->hash));
    this->expectReply("OK", 2, this->hash);
    ASSERT_EQ(this->verifier->receivedExpectedHashes.size(), 1u);
    EXPECT_EQ(this->verifier->receivedExpectedHashes[0].size(), 32u);
    EXPECT_EQ(this->verifier->receivedSignatures[0].size(), 64u);
}

TEST_F(SignatureVerifyInterfaceTest, EmptyTransferCanFinish) {
    this->start();
    this->send(beginCommand(this->hash, 0));
    this->send(finishCommand(this->hash));
    this->expectReply("OK", 0, this->hash);
    EXPECT_EQ(this->verifier->receivedData.size(), 0u);
    EXPECT_EQ(this->verifier->finishCallCount, 1u);
}

TEST_F(SignatureVerifyInterfaceTest, DuplicateChunkReacksWithoutRehashing) {
    this->start();
    this->send(beginCommand(this->hash, 3));
    this->send(chunkCommand(this->hash, 0, {0x11, 0x22}));
    this->send(chunkCommand(this->hash, 0, {0x99}));
    this->expectReply("PROGRESS", 2, this->hash);
    EXPECT_EQ(this->verifier->appendCallCount, 1u);
    EXPECT_EQ(this->verifier->abortCallCount, 0u);
}

TEST_F(SignatureVerifyInterfaceTest, GapAndPartialOverlapAbortTransfer) {
    this->start();
    this->send(beginCommand(this->hash, 8));
    this->send(chunkCommand(this->hash, 0, {0x11, 0x22}));
    this->send(chunkCommand(this->hash, 3, {0x33}));
    this->expectReply("ERROR invalid offset", 2, this->hash);
    EXPECT_EQ(this->verifier->abortCallCount, 1u);
    this->send(finishCommand(this->hash));
    this->expectReply("ERROR no active transfer", 0, this->hash);

    this->send(beginCommand(this->hash, 8));
    this->send(chunkCommand(this->hash, 0, {0x11, 0x22}));
    this->send(chunkCommand(this->hash, 1, {0x22, 0x33}));
    this->expectReply("ERROR invalid offset", 2, this->hash);
}

TEST_F(SignatureVerifyInterfaceTest, ExcessChunkAbortsTransfer) {
    this->start();
    this->send(beginCommand(this->hash, 1));
    this->send(chunkCommand(this->hash, 0, {0x11, 0x22}));
    this->expectReply("ERROR invalid offset", 0, this->hash);
    EXPECT_EQ(this->verifier->abortCallCount, 1u);
}

TEST_F(SignatureVerifyInterfaceTest, ConflictingSizeAbortsTransfer) {
    this->start();
    this->send(beginCommand(this->hash, 2));
    this->send(beginCommand(this->hash, 3));
    this->expectReply("ERROR conflicting size", 0, this->hash);
    EXPECT_EQ(this->verifier->abortCallCount, 1u);
}

TEST_F(SignatureVerifyInterfaceTest, DifferentHashBeginReplacesActiveTransfer) {
    this->start();
    this->send(beginCommand(this->hash, 2));
    this->send(chunkCommand(this->hash, 0, {0x11}));
    this->send(beginCommand(this->otherHash, 1));
    this->expectReply("PROGRESS", 0, this->otherHash);
    EXPECT_EQ(this->verifier->beginCallCount, 2u);
    EXPECT_EQ(this->verifier->abortCallCount, 1u);
}

TEST_F(SignatureVerifyInterfaceTest, HashAndSignatureFailuresMapToFail) {
    this->start();
    this->verifier->finishResult = {
        SignatureStatus::invalid, SignatureError::hashMismatch};
    this->send(beginCommand(this->hash, 0));
    this->send(finishCommand(this->hash));
    this->expectReply("FAIL hash mismatch", 0, this->hash);

    this->verifier->finishResult = {
        SignatureStatus::invalid, SignatureError::invalidSignature};
    this->send(beginCommand(this->hash, 0));
    this->send(finishCommand(this->hash));
    this->expectReply("FAIL invalid signature", 0, this->hash);
}

TEST_F(SignatureVerifyInterfaceTest, RetainedBeginErrorsNeverEmitProgress) {
    const std::vector<std::pair<SignatureError, std::string>> errors = {
        {SignatureError::invalidCertificate, "ERROR invalid certificate"},
        {SignatureError::unsupportedKey, "ERROR unsupported key"},
        {SignatureError::certificateUnavailable,
         "ERROR certificate unavailable"},
        {SignatureError::internalError, "ERROR internal error"},
        {SignatureError::invalidState, "ERROR invalid state"},
    };
    this->start();
    for (const auto& error : errors) {
        this->verifier->beginError = error.first;
        this->send(beginCommand(this->hash, 0));
        EXPECT_EQ(
            this->interface.storedValue,
            (std::vector<std::string>{error.second, "0", this->hash}));
        EXPECT_EQ(this->verifier->appendCallCount, 0u);
    }
    EXPECT_EQ(this->verifier->beginCallCount, errors.size());
    this->verifier->beginError = SignatureError::certificateUnavailable;
    this->send(beginCommand(this->hash, 0));
    this->send(beginCommand(this->hash, 0));
    EXPECT_EQ(
        this->interface.storedValue,
        (std::vector<std::string>{
            "ERROR certificate unavailable", "0", this->hash}));
    EXPECT_EQ(this->verifier->beginCallCount, errors.size() + 2);
}

TEST_F(
    SignatureVerifyInterfaceTest,
    MalformedAndOversizedErrorsPreserveActiveIdentity) {
    this->start();
    this->send(beginCommand(this->hash, 2));
    this->send(chunkCommand(this->hash, 0, {0x11}));
    this->send("not json");
    this->expectReply("ERROR invalid JSON", 1, this->hash);
    EXPECT_EQ(this->verifier->abortCallCount, 1u);

    this->send(beginCommand(this->hash, 2));
    this->send(chunkCommand(this->hash, 0, {0x11}));
    this->execute(std::string(513, 'x'));
    this->update();
    this->expectReply("ERROR command too large", 1, this->hash);
}

TEST_F(SignatureVerifyInterfaceTest, BurstOverflowReportsSnapshotAndAborts) {
    this->start();
    this->send(beginCommand(this->hash, 3));
    this->send(chunkCommand(this->hash, 0, {0x11, 0x22}));
    this->execute(chunkCommand(this->hash, 2, {0x33}));
    this->execute(chunkCommand(this->hash, 2, {0x44}));
    this->update();
    this->expectReply("ERROR busy", 2, this->hash);
    EXPECT_EQ(this->verifier->finishCallCount, 0u);
    EXPECT_EQ(this->verifier->abortCallCount, 1u);
}

TEST_F(
    SignatureVerifyInterfaceTest, DuplicateFinishIgnoredAndSameHashCanRestart) {
    this->start();
    this->send(beginCommand(this->hash, 0));
    this->send(finishCommand(this->hash));
    const std::size_t eventCount = this->action->firedValues.size();
    this->send(finishCommand(this->hash));
    EXPECT_EQ(this->action->firedValues.size(), eventCount);
    EXPECT_EQ(this->verifier->finishCallCount, 1u);
    this->send(beginCommand(this->hash, 0));
    this->expectReply("PROGRESS", 0, this->hash);
    EXPECT_EQ(this->verifier->beginCallCount, 2u);
}

TEST_F(SignatureVerifyInterfaceTest, PrematureFinishAbortsTransfer) {
    this->start();
    this->send(beginCommand(this->hash, 2));
    this->send(chunkCommand(this->hash, 0, {0x11}));
    this->send(finishCommand(this->hash));
    this->expectReply("ERROR incomplete transfer", 1, this->hash);
    EXPECT_EQ(this->verifier->finishCallCount, 0u);
    EXPECT_EQ(this->verifier->abortCallCount, 1u);
}

TEST_F(SignatureVerifyInterfaceTest, WrongTransferHashAbortsActiveTransfer) {
    this->start();
    this->send(beginCommand(this->hash, 1));
    this->send(chunkCommand(this->otherHash, 0, {0x11}));
    this->expectReply("ERROR transfer mismatch", 0, this->hash);
    EXPECT_EQ(this->verifier->abortCallCount, 1u);
}

TEST_F(SignatureVerifyInterfaceTest, MapsEveryStreamParserError) {
    this->start();
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"not json", "ERROR invalid JSON"},
        {"{}", "ERROR invalid fields"},
        {beginCommand("ABC", 0), "ERROR invalid hash"},
        {"{\"type\":\"chunk\",\"hash\":\"" + this->hash +
             "\",\"offset\":0,\"data\":\"!!\"}",
         "ERROR invalid base64"},
        {"{\"type\":\"chunk\",\"hash\":\"" + this->hash +
             "\",\"offset\":0,\"data\":\"" +
             base64Encode(std::vector<uint8_t>(257, 0x11)) + "\"}",
         "ERROR data too large"},
        {"{\"type\":\"finish\",\"hash\":\"" + this->hash +
             "\",\"signature\":\"AA==\"}",
         "ERROR invalid signature length"},
    };
    for (const auto& item : cases) {
        this->send(item.first);
        EXPECT_EQ(
            this->interface.storedValue,
            (std::vector<std::string>{item.second, "0", ""}));
    }
}

TEST_F(SignatureVerifyInterfaceTest, ReceiverTimesOutAtThirtySeconds) {
    this->start();
    this->send(beginCommand(this->hash, 1));
    this->esp.delay(29999);
    this->update();
    this->expectReply("PROGRESS", 0, this->hash);
    this->esp.delay(1);
    this->update();
    this->expectReply("ERROR timeout", 0, this->hash);
}

TEST_F(SignatureVerifyInterfaceTest, TimeoutElapsedMathWrapsAtUint32Boundary) {
    this->start();
    this->esp.delay(UINT32_MAX - 10000UL);
    this->send(beginCommand(this->hash, 1));
    this->esp.delay(29999);
    this->update();
    this->expectReply("PROGRESS", 0, this->hash);
    this->esp.delay(1);
    this->update();
    this->expectReply("ERROR timeout", 0, this->hash);
}

TEST_F(
    SignatureVerifyInterfaceTest, ReconnectInvalidatesQueuedOldSessionChunk) {
    this->start();
    this->send(beginCommand(this->hash, 1));
    this->execute(chunkCommand(this->hash, 0, {0x11}));
    this->transport = {false, 1};
    this->transport = {true, 2};
    this->update();
    this->expectReply("ERROR disconnected", 0, this->hash);
    EXPECT_EQ(this->verifier->appendCallCount, 0u);
}

TEST_F(
    SignatureVerifyInterfaceTest,
    NewSessionBeginWaitsBehindDisconnectSnapshot) {
    this->start();
    this->send(beginCommand(this->hash, 1));
    this->send(chunkCommand(this->hash, 0, {0x11}));
    this->transport.generation = 1;
    this->execute(beginCommand(this->otherHash, 0));
    this->update();
    this->expectReply("ERROR disconnected", 1, this->hash);
    this->update();
    this->expectReply("PROGRESS", 0, this->otherHash);
}

TEST_F(SignatureVerifyInterfaceTest, WifiLossInvalidatesWithoutMqttLoop) {
    this->start();
    this->send(beginCommand(this->hash, 1));
    this->transport.connected = false;
    this->update();
    this->expectReply("ERROR disconnected", 0, this->hash);
}

TEST_F(SignatureVerifyInterfaceTest, DisconnectedInterfaceDoesNotAcceptBegin) {
    this->start();
    this->transport.connected = false;
    this->send(beginCommand(this->hash, 0));
    this->update();
    EXPECT_EQ(this->verifier->beginCallCount, 0u);
    EXPECT_EQ(this->action->firedValues.size(), 1u);
}

TEST_F(SignatureVerifyInterfaceTest, AppendAndFinishErrorsClearTransfer) {
    this->start();
    this->send(beginCommand(this->hash, 1));
    this->verifier->appendError = SignatureError::internalError;
    this->send(chunkCommand(this->hash, 0, {0x11}));
    this->expectReply("ERROR internal error", 0, this->hash);
    EXPECT_EQ(this->verifier->abortCallCount, 1u);

    this->verifier->appendError = SignatureError::none;
    this->verifier->finishResult = {
        SignatureStatus::error, SignatureError::internalError};
    this->send(beginCommand(this->hash, 0));
    this->send(finishCommand(this->hash));
    this->expectReply("ERROR internal error", 0, this->hash);
    const std::size_t finishCalls = this->verifier->finishCallCount;
    const std::size_t eventCount = this->action->firedValues.size();
    this->send(finishCommand(this->hash));
    EXPECT_EQ(this->verifier->finishCallCount, finishCalls);
    EXPECT_EQ(this->action->firedValues.size(), eventCount);
}

TEST_F(
    SignatureVerifyInterfaceTest, InvalidStateAndBackendErrorsUseErrorPrefix) {
    this->start();
    this->verifier->finishResult = {
        SignatureStatus::error, SignatureError::invalidState};
    this->send(beginCommand(this->hash, 0));
    this->send(finishCommand(this->hash));
    this->expectReply("ERROR invalid state", 0, this->hash);
}
