#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "InterfaceTestBase.hpp"
#include "common/SignatureVerifier.hpp"
#include "common/SignatureVerifyInterface.hpp"

// Tests for the portable signature verification interface state machine. The
// interface under test is real; only the verifier is faked (a scriptable
// stub that records exact byte lengths) and presentation is observed through
// a capturing Action. Real InterfaceConfig/Actions drive the stored values.
//
// Scheduling contract exercised here (one action per update, in priority
// order): emit the initial empty value once; then emit a pending result; then
// queued error events (coalesced "ERROR busy" first, then oversized-input
// error); then process at most one queued command per update. A command
// queued before the initial update is processed on the next update, so the
// initial empty event is always preserved.
//
// Limits mirrored from the envelope wire format: a command longer than 512
// bytes is rejected in execute() without being copied.

namespace {

constexpr std::size_t interfaceMaxCommandSize = 512;

const char* const signature64Base64 =
    "paWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWl"
    "paWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpQ==";

const char* const signature63Base64 =
    "paWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaW"
    "lpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWl";

const char* const oversizeDataBase64 =
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8"
    "gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0"
    "BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fY"
    "GFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+A"
    "gYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6C"
    "hoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wM"
    "HCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4"
    "eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/wI=";

std::vector<uint8_t> signature64Bytes() {
    std::vector<uint8_t> bytes(64, 0xA5);
    return bytes;
}

// RFC 4648 base64 encoder, test only: produces canonical output with
// padding so envelope commands can be built for arbitrary binary data.
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

std::string envelope(const std::string& data, const std::string& signature) {
    return "{\"data\":\"" + data + "\",\"signature\":\"" + signature + "\"}";
}

std::string commandWith(const std::vector<uint8_t>& data) {
    return envelope(base64Encode(data), signature64Base64);
}

std::vector<std::string> singleValue(const std::string& value) {
    return {value};
}

std::vector<uint8_t> copyBytes(SignatureBytes bytes) {
    std::vector<uint8_t> out;
    out.reserve(bytes.size);
    for (std::size_t index = 0; index < bytes.size; ++index) {
        out.push_back(bytes.data[index]);
    }
    return out;
}

// Scriptable verifier stub: returns the scripted result and records what it
// was asked about, with exact lengths.
class FakeVerifier : public SignatureVerifier {
public:
    SignatureResult nextResult{SignatureStatus::valid, SignatureError::none};
    std::vector<std::vector<uint8_t>> receivedData;
    std::vector<std::vector<uint8_t>> receivedSignature;
    std::size_t verifyCallCount = 0;

    SignatureResult verify(
        SignatureBytes data, SignatureBytes signature) override {
        ++this->verifyCallCount;
        this->receivedData.push_back(copyBytes(data));
        this->receivedSignature.push_back(copyBytes(signature));
        return this->nextResult;
    }
};

// The interface owns its verifier; this adapter forwards to the shared fake
// so the test keeps a handle to it.
class ForwardingVerifier : public SignatureVerifier {
public:
    explicit ForwardingVerifier(std::shared_ptr<FakeVerifier> target)
        : target(std::move(target)) {}

    SignatureResult verify(
        SignatureBytes data, SignatureBytes signature) override {
        return this->target->verify(data, signature);
    }

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
    std::shared_ptr<CapturingAction> action =
        std::make_shared<CapturingAction>();
    std::shared_ptr<FakeVerifier> verifier = std::make_shared<FakeVerifier>();

    SignatureVerifyInterfaceTest() {
        this->interface.name = "signature-verify";
        this->interface.actions.push_back(this->action);
        this->interface.interface = std::make_unique<SignatureVerifyInterface>(
            std::make_unique<ForwardingVerifier>(this->verifier));
        this->interface.interface->start();
    }

    void execute(const std::string& command) {
        this->interface.interface->execute(command);
    }

    // Verifies the event emitted after a command whose decoding fails: the
    // stable error message with three updates (initial empty, processing,
    // presentation) and no verifier call.
    void expectDecodeError(
        const std::string& command, const std::string& errorMessage) {
        this->execute(command);
        this->updateInterface();
        EXPECT_EQ(this->interface.storedValue, singleValue(""));
        this->updateInterface();
        this->updateInterface();
        EXPECT_EQ(this->interface.storedValue, singleValue(errorMessage));
        EXPECT_EQ(this->verifier->verifyCallCount, 0u);
        EXPECT_EQ(this->verifier->receivedData.size(), 0u);
    }
};

TEST_F(SignatureVerifyInterfaceTest, InitialStoredValueIsEmptyEvent) {
    // Before the first update nothing has been emitted yet.
    EXPECT_EQ(this->interface.storedValue, std::vector<std::string>());
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue(""));
    EXPECT_EQ(this->action->firedValues.size(), 1u);
    this->updateInterface();
    this->updateInterface();
    // Idle updates never repeat the initial event.
    EXPECT_EQ(this->interface.storedValue, singleValue(""));
    EXPECT_EQ(this->action->firedValues.size(), 1u);
}

TEST_F(SignatureVerifyInterfaceTest, ValidSignatureEmitsOk) {
    this->execute(commandWith({0x11}));
    this->updateInterface();  // Initial empty event.
    this->updateInterface();  // Processes the queued command, verifying once.
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
    this->updateInterface();  // Presents the pending result.
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    EXPECT_EQ(
        this->action->firedValues, (std::vector<std::vector<std::string>>{
                                       singleValue(""), singleValue("OK")}));
}

TEST_F(SignatureVerifyInterfaceTest, InvalidSignatureEmitsFail) {
    this->verifier->nextResult = {
        SignatureStatus::invalid, SignatureError::invalidSignature};
    this->execute(commandWith({0x11}));
    this->updateInterface();
    this->updateInterface();
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
    this->updateInterface();
    EXPECT_EQ(
        this->interface.storedValue, singleValue("FAIL invalid signature"));
    EXPECT_EQ(
        this->action->firedValues,
        (std::vector<std::vector<std::string>>{
            singleValue(""), singleValue("FAIL invalid signature")}));
}

TEST_F(SignatureVerifyInterfaceTest, VerifierErrorResultsInStableErrorEvent) {
    // An initialization failure makes the verifier fail closed with its
    // retained error; the interface must present each code as a stable
    // string, one verification per case.
    const std::vector<std::pair<SignatureError, std::string>> cases = {
        {SignatureError::invalidCertificate, "ERROR invalid certificate"},
        {SignatureError::unsupportedKey, "ERROR unsupported key"},
        {SignatureError::certificateUnavailable,
         "ERROR certificate unavailable"},
        {SignatureError::invalidSignatureLength,
         "ERROR invalid signature length"},
        {SignatureError::internalError, "ERROR internal error"},
    };
    std::size_t expectedCalls = 0;
    for (const auto& caseValue : cases) {
        SCOPED_TRACE(caseValue.second);
        this->verifier->nextResult = {SignatureStatus::error, caseValue.first};
        this->execute(commandWith({0x11}));
        this->updateInterface();
        ++expectedCalls;
        this->updateInterface();
        this->updateInterface();
        EXPECT_EQ(this->interface.storedValue, singleValue(caseValue.second));
        EXPECT_EQ(this->verifier->verifyCallCount, expectedCalls);
        this->updateInterface();
        EXPECT_EQ(this->verifier->verifyCallCount, expectedCalls);
    }
}

TEST_F(SignatureVerifyInterfaceTest, InvalidJsonNeverCallsVerifier) {
    this->expectDecodeError("not json", "ERROR invalid JSON");
}

TEST_F(SignatureVerifyInterfaceTest, MalformedJsonSyntaxNeverCallsVerifier) {
    this->expectDecodeError("{\"data\":,}", "ERROR invalid JSON");
}

TEST_F(SignatureVerifyInterfaceTest, MissingFieldsNeverCallsVerifier) {
    this->expectDecodeError("{}", "ERROR invalid fields");
}

TEST_F(SignatureVerifyInterfaceTest, ExtraFieldsNeverCallsVerifier) {
    this->expectDecodeError(
        "{\"data\":\"\",\"signature\":\"\",\"extra\":\"x\"}",
        "ERROR invalid fields");
}

TEST_F(SignatureVerifyInterfaceTest, InvalidBase64NeverCallsVerifier) {
    this->expectDecodeError(
        envelope("!!", signature64Base64), "ERROR invalid base64");
}

TEST_F(SignatureVerifyInterfaceTest, DataTooLargeNeverCallsVerifier) {
    this->expectDecodeError(
        envelope(oversizeDataBase64, signature64Base64),
        "ERROR data too large");
}

TEST_F(SignatureVerifyInterfaceTest, InvalidSignatureLengthNeverCallsVerifier) {
    this->expectDecodeError(
        envelope("", signature63Base64), "ERROR invalid signature length");
}

TEST_F(SignatureVerifyInterfaceTest, EncoderMatchesSignatureFixture) {
    // Sanity check for the test's own base64 encoder against the known-good
    // signature fixture used by the envelope decoder tests.
    EXPECT_EQ(base64Encode(signature64Bytes()), signature64Base64);
}

TEST_F(SignatureVerifyInterfaceTest, BinaryBytesPassedWithExactLengths) {
    const std::vector<uint8_t> data{0x00, 0xFF, 0x01, 0x80, 0x7F};
    this->execute(commandWith(data));
    this->updateInterface();
    this->updateInterface();
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
    EXPECT_EQ(
        this->verifier->receivedData,
        (std::vector<std::vector<uint8_t>>{data}));
    EXPECT_EQ(
        this->verifier->receivedSignature,
        (std::vector<std::vector<uint8_t>>{signature64Bytes()}));

    // Empty data is allowed and must reach the verifier with length zero.
    this->execute(envelope("", signature64Base64));
    this->updateInterface();
    EXPECT_EQ(this->verifier->verifyCallCount, 2u);
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    EXPECT_EQ(
        this->verifier->receivedData,
        (std::vector<std::vector<uint8_t>>{data, {}}));
    EXPECT_EQ(this->verifier->receivedSignature[1], signature64Bytes());
}

TEST_F(SignatureVerifyInterfaceTest, TwoEqualValidResultsFireTwice) {
    const std::string command = commandWith({0x11});
    this->execute(command);
    this->updateInterface();
    this->updateInterface();
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    this->execute(command);
    this->updateInterface();
    this->updateInterface();
    EXPECT_EQ(this->verifier->verifyCallCount, 2u);
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    EXPECT_EQ(
        this->action->firedValues,
        (std::vector<std::vector<std::string>>{
            singleValue(""), singleValue("OK"), singleValue("OK")}));
}

TEST_F(SignatureVerifyInterfaceTest, BadThenGoodRecovers) {
    this->verifier->nextResult = {
        SignatureStatus::invalid, SignatureError::invalidSignature};
    this->execute(commandWith({0x11}));
    this->updateInterface();
    this->updateInterface();
    this->updateInterface();
    EXPECT_EQ(
        this->interface.storedValue, singleValue("FAIL invalid signature"));

    this->verifier->nextResult = {SignatureStatus::valid, SignatureError::none};
    this->execute(commandWith({0x22}));
    this->updateInterface();
    this->updateInterface();
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    EXPECT_EQ(
        this->action->firedValues,
        (std::vector<std::vector<std::string>>{
            singleValue(""), singleValue("FAIL invalid signature"),
            singleValue("OK")}));
}

TEST_F(
    SignatureVerifyInterfaceTest,
    CommandBeforeFirstUpdatePreservesInitialEmptyEventAndQueuedCommand) {
    this->execute(commandWith({0x11}));  // Before the initial update.
    this->updateInterface();
    // The queued command did not displace the initial empty event and was
    // not processed yet.
    EXPECT_EQ(this->interface.storedValue, singleValue(""));
    EXPECT_EQ(this->verifier->verifyCallCount, 0u);
    this->updateInterface();
    // Processed on the next update; nothing is presented yet.
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
    EXPECT_EQ(this->action->firedValues.size(), 1u);
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    EXPECT_EQ(
        this->action->firedValues, (std::vector<std::vector<std::string>>{
                                       singleValue(""), singleValue("OK")}));
}

TEST_F(SignatureVerifyInterfaceTest, OnePendingSlotRetainsFirstCommand) {
    this->execute(commandWith({0x11}));
    this->execute(commandWith({0x22}));  // Overflow: dropped.
    this->updateInterface();
    this->updateInterface();
    // The first command is the only one ever verified.
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
    EXPECT_EQ(
        this->verifier->receivedData,
        (std::vector<std::vector<uint8_t>>{{0x11}}));
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    this->updateInterface();
    // A single coalesced busy error follows the pending result.
    EXPECT_EQ(this->interface.storedValue, singleValue("ERROR busy"));
    this->updateInterface();
    this->updateInterface();
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
    EXPECT_EQ(
        this->action->firedValues,
        (std::vector<std::vector<std::string>>{
            singleValue(""), singleValue("OK"), singleValue("ERROR busy")}));
}

TEST_F(SignatureVerifyInterfaceTest, OverflowCoalescesToOneBusyError) {
    this->execute(commandWith({0x11}));
    for (int i = 0; i < 20; ++i) {
        this->execute(commandWith({0x22, static_cast<uint8_t>(i)}));
    }
    this->updateInterface();
    this->updateInterface();
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("ERROR busy"));
    this->updateInterface();
    this->updateInterface();
    this->updateInterface();
    // No unbounded growth: 21 arrivals collapse into one dropped-command
    // error, and later idle updates stay silent.
    EXPECT_EQ(
        this->action->firedValues,
        (std::vector<std::vector<std::string>>{
            singleValue(""), singleValue("OK"), singleValue("ERROR busy")}));
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
}

TEST_F(
    SignatureVerifyInterfaceTest,
    BusyNotificationNotStarvedAndNoSecondVerificationInSameUpdate) {
    this->execute(commandWith({0x11}));
    this->updateInterface();  // Initial empty event; A is still queued.
    this->execute(commandWith({0x22}));  // Slot held by A: B overflows.
    this->updateInterface();             // Processes A.
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
    this->execute(commandWith({0x33}));  // Arrival while busy: retained.
    this->updateInterface();
    // The pending result is presented despite the retained command, and
    // nothing is verified in the same update as the presentation.
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("ERROR busy"));
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
    this->updateInterface();
    // The retained command is processed after the notification was emitted.
    EXPECT_EQ(this->verifier->verifyCallCount, 2u);
    EXPECT_EQ(
        this->verifier->receivedData,
        (std::vector<std::vector<uint8_t>>{{0x11}, {0x33}}));
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    EXPECT_EQ(
        this->action->firedValues,
        (std::vector<std::vector<std::string>>{
            singleValue(""), singleValue("OK"), singleValue("ERROR busy"),
            singleValue("OK")}));
}

TEST_F(
    SignatureVerifyInterfaceTest, RetainedCommandProcessedAfterNotification) {
    this->execute(commandWith({0x11}));
    this->updateInterface();
    this->updateInterface();
    // Arrival while the result notification is pending: retained, not
    // dropped, because the slot was free.
    this->execute(commandWith({0x22}));
    this->updateInterface();
    // The notification is emitted first; the retained command cannot starve
    // it and is not verified in the same update.
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
    this->updateInterface();
    EXPECT_EQ(this->verifier->verifyCallCount, 2u);
    EXPECT_EQ(
        this->verifier->receivedData,
        (std::vector<std::vector<uint8_t>>{{0x11}, {0x22}}));
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    EXPECT_EQ(
        this->action->firedValues,
        (std::vector<std::vector<std::string>>{
            singleValue(""), singleValue("OK"), singleValue("OK")}));
}

TEST_F(SignatureVerifyInterfaceTest, OversizedInputQueuesOnlyErrorWithoutCopy) {
    const std::string oversized(interfaceMaxCommandSize + 1, 'x');
    this->execute(oversized);
    // The oversized input did not occupy the command slot and did not copy
    // the payload: only an error code was queued.
    this->execute(commandWith({0x22}));
    this->updateInterface();
    this->updateInterface();
    EXPECT_EQ(
        this->interface.storedValue, singleValue("ERROR command too large"));
    EXPECT_EQ(this->verifier->verifyCallCount, 0u);
    this->updateInterface();
    // The normal command queued after the oversized one is processed.
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
    EXPECT_EQ(
        this->verifier->receivedData,
        (std::vector<std::vector<uint8_t>>{{0x22}}));
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    EXPECT_EQ(
        this->action->firedValues,
        (std::vector<std::vector<std::string>>{
            singleValue(""), singleValue("ERROR command too large"),
            singleValue("OK")}));
}