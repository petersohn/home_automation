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
// Scheduling contract exercised here: emit the initial empty value once;
// later updates process and emit one command result immediately. Multiple
// execute() calls between updates discard the entire burst and emit one
// "ERROR busy" without verification.
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

    // Verifies same-update presentation for a decode error and no verifier
    // call. The first update remains reserved for the initial empty event.
    void expectDecodeError(
        const std::string& command, const std::string& errorMessage) {
        this->execute(command);
        this->updateInterface();
        EXPECT_EQ(this->interface.storedValue, singleValue(""));
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
    this->updateInterface();  // Verifies and emits in this same update.
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
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
    this->updateInterface();
    for (const auto& caseValue : cases) {
        SCOPED_TRACE(caseValue.second);
        this->verifier->nextResult = {SignatureStatus::error, caseValue.first};
        this->execute(commandWith({0x11}));
        this->updateInterface();
        ++expectedCalls;
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
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    EXPECT_EQ(
        this->verifier->receivedData,
        (std::vector<std::vector<uint8_t>>{data, {}}));
    EXPECT_EQ(this->verifier->receivedSignature[1], signature64Bytes());
}

TEST_F(SignatureVerifyInterfaceTest, TwoEqualValidResultsFireTwice) {
    const std::string command = commandWith({0x11});
    this->updateInterface();
    this->execute(command);
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    this->execute(command);
    this->updateInterface();
    EXPECT_EQ(this->verifier->verifyCallCount, 2u);
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    EXPECT_EQ(
        this->action->firedValues,
        (std::vector<std::vector<std::string>>{
            singleValue(""), singleValue("OK"), singleValue("OK")}));
}

TEST_F(SignatureVerifyInterfaceTest, BadThenGoodRecovers) {
    this->verifier->nextResult = {
        SignatureStatus::invalid, SignatureError::invalidSignature};
    this->updateInterface();
    this->execute(commandWith({0x11}));
    this->updateInterface();
    EXPECT_EQ(
        this->interface.storedValue, singleValue("FAIL invalid signature"));

    this->verifier->nextResult = {SignatureStatus::valid, SignatureError::none};
    this->execute(commandWith({0x22}));
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
    // Processed and presented together on the next update.
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    EXPECT_EQ(
        this->action->firedValues, (std::vector<std::vector<std::string>>{
                                       singleValue(""), singleValue("OK")}));
}

TEST_F(SignatureVerifyInterfaceTest, MultipleCommandsDiscardWholeBurst) {
    this->updateInterface();
    this->execute(commandWith({0x11}));
    this->execute(commandWith({0x22}));
    this->execute(commandWith({0x33}));
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("ERROR busy"));
    EXPECT_EQ(this->verifier->verifyCallCount, 0u);

    // Arrivals remain discarded until the update resets the busy burst.
    this->execute(commandWith({0x44}));
    this->execute(commandWith({0x55}));
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("ERROR busy"));
    EXPECT_EQ(this->verifier->verifyCallCount, 0u);

    this->execute(commandWith({0x66}));
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
    EXPECT_EQ(
        this->verifier->receivedData,
        (std::vector<std::vector<uint8_t>>{{0x66}}));
}

TEST_F(SignatureVerifyInterfaceTest, OversizedAndValidBurstReportsBusy) {
    this->updateInterface();
    const std::string oversized(interfaceMaxCommandSize + 1, 'x');
    this->execute(oversized);
    this->execute(commandWith({0x22}));
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("ERROR busy"));
    EXPECT_EQ(this->verifier->verifyCallCount, 0u);

    // A lone oversized arrival retains its specific error and no payload.
    this->execute(oversized);
    this->updateInterface();
    EXPECT_EQ(
        this->interface.storedValue, singleValue("ERROR command too large"));
    EXPECT_EQ(this->verifier->verifyCallCount, 0u);

    this->execute(commandWith({0x33}));
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("OK"));
    EXPECT_EQ(this->verifier->verifyCallCount, 1u);
}

TEST_F(SignatureVerifyInterfaceTest, TwoOversizedArrivalsAreBusy) {
    this->updateInterface();
    const std::string oversized(interfaceMaxCommandSize + 1, 'x');
    this->execute(oversized);
    this->execute(oversized);
    this->execute(commandWith({0x11}));
    this->updateInterface();
    EXPECT_EQ(this->interface.storedValue, singleValue("ERROR busy"));
    EXPECT_EQ(this->verifier->verifyCallCount, 0u);
}
