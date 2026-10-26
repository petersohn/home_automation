#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "FakeEspApi.hpp"
#include "common/MqttTransportState.hpp"
#include "common/SignatureVerifier.hpp"
#include "common/SignedStreamReceiver.hpp"

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

const std::string hash(64, 'a');

std::string beginCommand(const std::string& commandHash, uint32_t size) {
    return "{\"type\":\"begin\",\"hash\":\"" + commandHash +
           "\",\"size\":" + std::to_string(size) + "}";
}

std::string chunkCommand(
    const std::string& commandHash, uint32_t offset,
    const std::vector<uint8_t>& bytes) {
    return "{\"type\":\"chunk\",\"hash\":\"" + commandHash +
           "\",\"offset\":" + std::to_string(offset) + ",\"data\":\"" +
           base64Encode(bytes) + "\"}";
}

std::string finishCommand(const std::string& commandHash) {
    return "{\"type\":\"finish\",\"hash\":\"" + commandHash +
           "\",\"signature\":\"" +
           base64Encode(std::vector<uint8_t>(64, 0xA5)) + "\"}";
}

class FakeVerifier : public SignatureVerifier {
public:
    SignatureError beginError = SignatureError::none;
    SignatureError appendError = SignatureError::none;
    SignatureResult finishResult{SignatureStatus::valid, SignatureError::none};
    std::size_t beginCalls = 0;
    std::size_t appendCalls = 0;
    std::size_t finishCalls = 0;
    std::size_t abortCalls = 0;
    std::vector<std::vector<uint8_t>> receivedData;
    std::vector<std::vector<uint8_t>> receivedHashes;
    std::vector<std::vector<uint8_t>> receivedSignatures;

    SignatureError begin() override {
        ++this->beginCalls;
        return this->beginError;
    }

    SignatureError append(SignatureBytes data) override {
        ++this->appendCalls;
        this->receivedData.push_back(copyBytes(data));
        return this->appendError;
    }

    SignatureResult finish(
        SignatureBytes expectedHash, SignatureBytes signature) override {
        ++this->finishCalls;
        this->receivedHashes.push_back(copyBytes(expectedHash));
        this->receivedSignatures.push_back(copyBytes(signature));
        return this->finishResult;
    }

    void abort() override { ++this->abortCalls; }
};

class FakeSink : public SignedStreamSink {
public:
    bool acceptBegin = true;
    bool acceptAppend = true;
    std::string completionStatus = "STAGED";
    std::size_t beginCalls = 0;
    std::size_t appendCalls = 0;
    std::size_t completeCalls = 0;
    std::size_t abortCalls = 0;
    std::vector<uint32_t> sizes;
    std::vector<std::vector<uint8_t>> receivedData;
    std::vector<SignedStreamReply> completedReplies;

    bool begin(uint32_t size) override {
        ++this->beginCalls;
        this->sizes.push_back(size);
        return this->acceptBegin;
    }

    bool append(SignatureBytes data) override {
        ++this->appendCalls;
        this->receivedData.push_back(copyBytes(data));
        return this->acceptAppend;
    }

    std::string complete(const SignedStreamReply& verified) override {
        ++this->completeCalls;
        this->completedReplies.push_back(verified);
        return this->completionStatus;
    }

    void abort() override { ++this->abortCalls; }
};

class ReceiverHarness {
public:
    FakeEspApi esp;
    FakeSink sink;
    MqttTransportState transport{true, 0};
    FakeVerifier* verifier = nullptr;
    std::unique_ptr<SignedStreamReceiver> receiver;

    ReceiverHarness() { this->replaceReceiver(); }

    void replaceReceiver() {
        auto verifier = std::make_unique<FakeVerifier>();
        this->verifier = verifier.get();
        this->receiver = std::make_unique<SignedStreamReceiver>(
            this->esp, std::move(verifier), this->sink,
            [this]() { return this->transport; });
    }

    std::optional<SignedStreamReply> send(const std::string& command) {
        this->receiver->execute(command);
        return this->receiver->update();
    }
};

}  // namespace

TEST(SignedStreamReceiverTest, SinkReceivesEachByteOnce) {
    ReceiverHarness receiver;
    auto reply = receiver.send(beginCommand(hash, 2));
    ASSERT_TRUE(reply.has_value());
    ASSERT_EQ(reply->status, "PROGRESS");

    const std::string chunk = chunkCommand(hash, 0, {0x11, 0x22});
    const auto firstReply = receiver.send(chunk);
    ASSERT_TRUE(firstReply.has_value());
    const auto secondReply = receiver.send(chunk);
    ASSERT_TRUE(secondReply.has_value());

    EXPECT_EQ(receiver.sink.appendCalls, 1u);
    ASSERT_EQ(receiver.sink.receivedData.size(), 1u);
    EXPECT_EQ(
        receiver.sink.receivedData[0], (std::vector<uint8_t>{0x11, 0x22}));
    EXPECT_EQ(receiver.sink.completeCalls, 0u);
    EXPECT_EQ(secondReply->status, "PROGRESS");
    EXPECT_EQ(secondReply->transferred, firstReply->transferred);
}

TEST(SignedStreamReceiverTest, SinkFinishOnlyAfterValidSignature) {
    ReceiverHarness receiver;
    auto reply = receiver.send(beginCommand(hash, 0));
    ASSERT_TRUE(reply.has_value());
    ASSERT_EQ(reply->status, "PROGRESS");

    receiver.verifier->finishResult = {
        SignatureStatus::invalid, SignatureError::hashMismatch};
    reply = receiver.send(finishCommand(hash));
    ASSERT_TRUE(reply.has_value());
    EXPECT_NE(reply->status, "STAGED");
    EXPECT_EQ(receiver.sink.completeCalls, 0u);

    receiver.verifier->finishResult = {
        SignatureStatus::invalid, SignatureError::invalidSignature};
    reply = receiver.send(beginCommand(hash, 0));
    ASSERT_TRUE(reply.has_value());
    ASSERT_EQ(reply->status, "PROGRESS");
    reply = receiver.send(finishCommand(hash));
    ASSERT_TRUE(reply.has_value());
    EXPECT_NE(reply->status, "STAGED");
    EXPECT_EQ(receiver.sink.completeCalls, 0u);

    receiver.verifier->finishResult = {
        SignatureStatus::valid, SignatureError::none};
    reply = receiver.send(beginCommand(hash, 0));
    ASSERT_TRUE(reply.has_value());
    ASSERT_EQ(reply->status, "PROGRESS");
    reply = receiver.send(finishCommand(hash));
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->status, "STAGED");
    ASSERT_EQ(receiver.sink.completeCalls, 1u);
    ASSERT_EQ(receiver.sink.completedReplies.size(), 1u);
    EXPECT_EQ(receiver.sink.completedReplies[0].status, "OK");
    EXPECT_EQ(receiver.sink.completedReplies[0].transferred, 0u);
    EXPECT_EQ(receiver.sink.completedReplies[0].hash, hash);
}

TEST(SignedStreamReceiverTest, SinkFailureAborts) {
    ReceiverHarness receiver;
    receiver.sink.acceptBegin = false;
    auto reply = receiver.send(beginCommand(hash, 2));
    ASSERT_TRUE(reply.has_value());
    EXPECT_NE(reply->status, "PROGRESS");
    EXPECT_EQ(reply->transferred, 0u);
    EXPECT_EQ(receiver.sink.abortCalls, 1u);

    receiver.sink.acceptBegin = true;
    receiver.sink.acceptAppend = false;
    reply = receiver.send(beginCommand(hash, 2));
    ASSERT_TRUE(reply.has_value());
    ASSERT_EQ(reply->status, "PROGRESS");
    reply = receiver.send(chunkCommand(hash, 0, {0x11}));
    ASSERT_TRUE(reply.has_value());
    EXPECT_NE(reply->status, "PROGRESS");
    EXPECT_EQ(reply->transferred, 0u);
    EXPECT_EQ(receiver.sink.appendCalls, 1u);
    EXPECT_EQ(receiver.sink.abortCalls, 2u);
}

TEST(
    SignedStreamReceiverTest,
    CompletionFailureAbortsAndSuppressesDuplicateFinish) {
    ReceiverHarness receiver;
    receiver.sink.completionStatus = "ERROR storage failure";
    ASSERT_TRUE(receiver.send(beginCommand(hash, 1)).has_value());
    ASSERT_TRUE(receiver.send(chunkCommand(hash, 0, {0x11})).has_value());

    const auto reply = receiver.send(finishCommand(hash));
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->status, "ERROR storage failure");
    EXPECT_EQ(reply->transferred, 1u);
    EXPECT_EQ(reply->hash, hash);
    EXPECT_EQ(receiver.sink.completeCalls, 1u);
    EXPECT_EQ(receiver.sink.abortCalls, 1u);
    EXPECT_FALSE(receiver.send(finishCommand(hash)).has_value());
    EXPECT_EQ(receiver.sink.completeCalls, 1u);
    EXPECT_EQ(receiver.sink.abortCalls, 1u);
}

TEST(SignedStreamReceiverTest, DisconnectBusyAndTimeoutPreserved) {
    {
        ReceiverHarness receiver;
        auto reply = receiver.send(beginCommand(hash, 1));
        ASSERT_TRUE(reply.has_value());
        ASSERT_EQ(reply->status, "PROGRESS");
        receiver.receiver->execute(chunkCommand(hash, 0, {0x11}));
        receiver.transport = {false, 1};
        receiver.transport = {true, 1};
        reply = receiver.receiver->update();
        ASSERT_TRUE(reply.has_value());
        EXPECT_EQ(reply->status, "ERROR disconnected");
        EXPECT_EQ(reply->transferred, 0u);
        EXPECT_EQ(receiver.sink.appendCalls, 0u);
    }

    {
        ReceiverHarness receiver;
        auto reply = receiver.send(beginCommand(hash, 3));
        ASSERT_TRUE(reply.has_value());
        ASSERT_EQ(reply->status, "PROGRESS");
        reply = receiver.send(chunkCommand(hash, 0, {0x11}));
        ASSERT_TRUE(reply.has_value());
        ASSERT_EQ(reply->transferred, 1u);
        receiver.receiver->execute(chunkCommand(hash, 1, {0x22}));
        receiver.receiver->execute(chunkCommand(hash, 2, {0x33}));
        reply = receiver.receiver->update();
        ASSERT_TRUE(reply.has_value());
        EXPECT_EQ(reply->status, "ERROR busy");
        EXPECT_EQ(reply->transferred, 1u);
        EXPECT_EQ(receiver.sink.appendCalls, 1u);
    }

    {
        ReceiverHarness receiver;
        auto reply = receiver.send(beginCommand(hash, 1));
        ASSERT_TRUE(reply.has_value());
        receiver.esp.delay(30000);
        reply = receiver.receiver->update();
        ASSERT_TRUE(reply.has_value());
        EXPECT_EQ(reply->status, "ERROR timeout");
        EXPECT_EQ(reply->transferred, 0u);
    }
}

TEST(SignedStreamReceiverTest, DuplicateBeginDoesNotExtendTimeout) {
    ReceiverHarness receiver;
    auto reply = receiver.send(beginCommand(hash, 2));
    ASSERT_TRUE(reply.has_value());
    ASSERT_EQ(reply->status, "PROGRESS");
    reply = receiver.send(chunkCommand(hash, 0, {0x11, 0x22}));
    ASSERT_TRUE(reply.has_value());
    ASSERT_EQ(reply->transferred, 2u);

    receiver.esp.delay(15000);
    reply = receiver.send(beginCommand(hash, 2));
    ASSERT_TRUE(reply.has_value());
    const auto duplicateReply =
        receiver.send(chunkCommand(hash, 0, {0x11, 0x22}));
    ASSERT_TRUE(duplicateReply.has_value());
    EXPECT_EQ(duplicateReply->status, "PROGRESS");
    EXPECT_EQ(duplicateReply->transferred, 2u);
    EXPECT_EQ(receiver.sink.appendCalls, 1u);

    receiver.esp.delay(15000);
    reply = receiver.receiver->update();
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->status, "ERROR timeout");
    EXPECT_EQ(reply->transferred, 2u);
}
