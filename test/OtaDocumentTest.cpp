#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "common/ConfigParser.hpp"
#include "common/OtaDocument.hpp"
#include "common/OtaFile.hpp"
#include "tools/JsonParsing.hpp"

namespace {

class MemoryOtaFile final : public OtaFile {
public:
    explicit MemoryOtaFile(std::string input) : content(std::move(input)) {}

    int read() override {
        if (this->position == this->content.size()) {
            return -1;
        }
        return static_cast<unsigned char>(this->content[this->position++]);
    }

    size_t write(SignatureBytes bytes) override {
        if (bytes.size != 0) {
            this->content.append(
                reinterpret_cast<const char*>(bytes.data), bytes.size);
        }
        return bytes.size;
    }

    bool close() override { return true; }

    bool failed() const override { return false; }

private:
    std::string content;
    size_t position = 0;
};

constexpr const char* stationMac = "AA:BB:CC:DD:EE:FF";

std::string makeConfig(
    const std::string& version = "12",
    const std::string& prefix = "home/device") {
    return R"({"otaEnabled":true,"configVersion":)" + version +
           R"(,"servicePrefix":")" + prefix + R"("})";
}

std::string makeEnvelope(
    const std::string& config, const std::string& type = "device-config",
    const std::string& target = stationMac) {
    return R"({"type":")" + type + R"(","target":")" + target +
           R"(","config":)" + config + "}";
}

ParsedOtaDocument parseDocument(
    const std::string& input, const std::string& localMac = stationMac,
    std::optional<uint32_t> minimum = std::optional<uint32_t>{1}) {
    MemoryOtaFile file(input);
    return parseOtaDocument(file, localMac, minimum);
}

ArduinoJson::JsonObject& parseConfig(
    ArduinoJson::DynamicJsonBuffer& buffer, const std::string& text) {
    return buffer.parseObject(text);
}

}  // namespace

TEST(OtaDocumentTest, RequiresBooleanOtaEnablement) {
    const std::vector<std::pair<std::string, bool>> cases = {
        {R"({"otaEnabled":true,"configVersion":12,"servicePrefix":"home///"})",
         true},
        {R"({"otaEnabled":false,"configVersion":12,"servicePrefix":"home"})",
         false},
        {R"({"otaEnabled":1,"configVersion":12,"servicePrefix":"home"})",
         false},
        {R"({"otaEnabled":"true","configVersion":12,"servicePrefix":"home"})",
         false},
        {R"({"otaEnabled":null,"configVersion":12,"servicePrefix":"home"})",
         false},
        {R"({"configVersion":12,"servicePrefix":"home"})", false},
    };

    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.first);
        ArduinoJson::DynamicJsonBuffer buffer{1024};
        auto& root = parseConfig(buffer, testCase.first);
        ASSERT_NE(root, ArduinoJson::JsonObject::invalid());

        const auto policy = inspectOtaConfig(root, true, false);
        EXPECT_EQ(policy.valid, testCase.second);
        if (testCase.second) {
            EXPECT_EQ(policy.version, 12u);
            EXPECT_EQ(policy.servicePrefix, "home");
        }
    }
}

TEST(OtaDocumentTest, VersionTypeAndUint32Boundaries) {
    const std::vector<std::string> invalidVersions = {
        "1.0", "1.5", "-1", "4294967296", "true", "\"12\"", "null"};
    for (const std::string& version : invalidVersions) {
        SCOPED_TRACE(version);
        ArduinoJson::DynamicJsonBuffer buffer{1024};
        auto& root = parseConfig(buffer, makeConfig(version));
        ASSERT_NE(root, ArduinoJson::JsonObject::invalid());
        EXPECT_FALSE(inspectOtaConfig(root, true, false).valid);
    }

    ArduinoJson::DynamicJsonBuffer maximumBuffer{1024};
    auto& maximumRoot = parseConfig(maximumBuffer, makeConfig("4294967295"));
    ASSERT_NE(maximumRoot, ArduinoJson::JsonObject::invalid());
    const auto maximumPolicy = inspectOtaConfig(maximumRoot, true, false);
    ASSERT_TRUE(maximumPolicy.valid);
    EXPECT_EQ(maximumPolicy.version, UINT32_MAX);

    const auto maximumAtMinimum = parseDocument(
        makeEnvelope(makeConfig("4294967295")), stationMac, UINT32_MAX);
    ASSERT_NE(maximumAtMinimum.config, nullptr);
    EXPECT_TRUE(maximumAtMinimum.policy.valid);
    EXPECT_EQ(maximumAtMinimum.policy.version, UINT32_MAX);
}

TEST(OtaDocumentTest, MacMatchesBytesNotName) {
    const auto lowerTarget = parseDocument(
        makeEnvelope(makeConfig(), "device-config", "aa:bb:cc:dd:ee:ff"));
    ASSERT_NE(lowerTarget.config, nullptr);
    EXPECT_TRUE(lowerTarget.policy.valid);

    for (const std::string target :
         {"AA:BB:CC:DD:EE:00", "station", "AA-BB-CC-DD-EE-FF"}) {
        SCOPED_TRACE(target);
        const auto result =
            parseDocument(makeEnvelope(makeConfig(), "device-config", target));
        EXPECT_EQ(result.config, nullptr);
        EXPECT_FALSE(result.policy.valid);
    }

    const auto invalidLocalMac =
        parseDocument(makeEnvelope(makeConfig()), "station", 1u);
    EXPECT_EQ(invalidLocalMac.config, nullptr);
    EXPECT_FALSE(invalidLocalMac.policy.valid);
}

TEST(OtaDocumentTest, RejectsWrongEnvelopeType) {
    const auto result =
        parseDocument(makeEnvelope(makeConfig(), "firmware", stationMac));

    EXPECT_EQ(result.config, nullptr);
    EXPECT_FALSE(result.policy.valid);
}

TEST(OtaDocumentTest, RejectsDisabledUpdatePath) {
    const std::vector<std::string> invalidPrefixes = {
        R"("")", R"("///")", "null", R"("#")", R"( "+" )"};
    for (const std::string& prefix : invalidPrefixes) {
        SCOPED_TRACE(prefix);
        ArduinoJson::DynamicJsonBuffer buffer{1024};
        auto& root = parseConfig(
            buffer,
            R"({"otaEnabled":true,"configVersion":12,"servicePrefix":)" +
                prefix + "}");
        ASSERT_NE(root, ArduinoJson::JsonObject::invalid());
        EXPECT_FALSE(inspectOtaConfig(root, true, false).valid);
    }
}

TEST(OtaDocumentTest, AcceptsLiteralAndNativeUnicodePrefixText) {
    const std::vector<std::pair<std::string, std::string>> cases = {
        {makeConfig("12", "home/u0000device"), "home/u0000device"},
        {R"({"otaEnabled":true,"configVersion":12,"servicePrefix":"home/\u0000device"})",
         "home/u0000device"}};

    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.first);
        ArduinoJson::DynamicJsonBuffer buffer{1024};
        auto& root = parseConfig(buffer, testCase.first);
        ASSERT_NE(root, ArduinoJson::JsonObject::invalid());

        const auto policy = inspectOtaConfig(root, true, false);
        ASSERT_TRUE(policy.valid);
        EXPECT_EQ(policy.servicePrefix, testCase.second);

        const auto document = parseDocument(makeEnvelope(testCase.first));
        ASSERT_NE(document.config, nullptr);
        ASSERT_TRUE(document.policy.valid);
        EXPECT_EQ(document.policy.servicePrefix, testCase.second);
    }
}

TEST(OtaDocumentTest, EnforcesTransportPrefixBoundary) {
    for (const auto& testCase :
         {std::pair<size_t, bool>{482, true}, {483, false}}) {
        SCOPED_TRACE(testCase.first);
        const std::string prefix(testCase.first, 'p');
        ArduinoJson::DynamicJsonBuffer buffer{2048};
        auto& root = parseConfig(buffer, makeConfig("12", prefix));
        ASSERT_NE(root, ArduinoJson::JsonObject::invalid());
        const auto policy = inspectOtaConfig(root, true, false);
        EXPECT_EQ(policy.valid, testCase.second);
        if (testCase.second) {
            EXPECT_EQ(policy.servicePrefix.size(), 482u);
        }
    }
}

TEST(OtaDocumentTest, RejectsReplayAndVersionRangeExhaustion) {
    const auto replay =
        parseDocument(makeEnvelope(makeConfig("8")), stationMac, 9u);
    EXPECT_EQ(replay.config, nullptr);
    EXPECT_FALSE(replay.policy.valid);

    const auto inclusiveMinimum =
        parseDocument(makeEnvelope(makeConfig("9")), stationMac, 9u);
    ASSERT_NE(inclusiveMinimum.config, nullptr);
    EXPECT_TRUE(inclusiveMinimum.policy.valid);

    const auto exhausted = parseDocument(
        makeEnvelope(makeConfig("4294967295")), stationMac, std::nullopt);
    EXPECT_EQ(exhausted.config, nullptr);
    EXPECT_FALSE(exhausted.policy.valid);
}

TEST(OtaDocumentTest, RejectsEmptyRootAndMissingConfig) {
    for (const std::string input : {"", "{}", R"({"type":"device-config"})"}) {
        SCOPED_TRACE(input);
        const auto result = parseDocument(input);
        EXPECT_EQ(result.config, nullptr);
        EXPECT_FALSE(result.policy.valid);
    }
}

TEST(OtaDocumentTest, RejectsTrailingGarbageAndNul) {
    const std::string valid = makeEnvelope(makeConfig());
    const auto trailingGarbage = parseDocument(valid + " trailing");
    EXPECT_EQ(trailingGarbage.config, nullptr);
    EXPECT_FALSE(trailingGarbage.policy.valid);

    const auto secondDocument = parseDocument(valid + " {}");
    EXPECT_EQ(secondDocument.config, nullptr);
    EXPECT_FALSE(secondDocument.policy.valid);

    std::string withNul = valid;
    withNul.push_back('\0');
    withNul += "tail";
    const auto nul = parseDocument(withNul);
    EXPECT_EQ(nul.config, nullptr);
    EXPECT_FALSE(nul.policy.valid);
}

TEST(OtaDocumentTest, WrapperPreservesConfigDepth) {
    std::string config =
        R"({"otaEnabled":true,"configVersion":12,"servicePrefix":"home","nested":)";
    for (uint8_t depth = 1; depth < ARDUINOJSON_DEFAULT_NESTING_LIMIT;
         ++depth) {
        config += R"({"nested":)";
    }
    config += "0";
    for (uint8_t depth = 1; depth < ARDUINOJSON_DEFAULT_NESTING_LIMIT;
         ++depth) {
        config += "}";
    }
    config += "}";

    const auto result = parseDocument(makeEnvelope(config));

    ASSERT_NE(result.config, nullptr);
    EXPECT_TRUE(result.policy.valid);
    EXPECT_EQ(result.policy.version, 12u);
}

TEST(OtaDocumentTest, AllowsInvalidInterfaceEntries) {
    const std::string config = R"({
        "otaEnabled":true,
        "configVersion":12,
        "servicePrefix":"home/device",
        "interfaces":[{"type":"input","name":"missing-pin"}]
    })";
    const auto result = parseDocument(makeEnvelope(config));

    ASSERT_NE(result.config, nullptr);
    ASSERT_TRUE(result.policy.valid);
    std::ostringstream debug;
    ConfigParser parser(debug);
    const auto parsed = parser.parseDeviceConfig(*result.config);
    EXPECT_TRUE(parsed.interfaces.empty());
}

TEST(OtaDocumentTest, ParsesUint32WithDeviceCompatibleJsonVariant) {
    const std::vector<std::pair<std::string, bool>> cases = {
        {"0", true},       {"4294967295", true},  {"1.0", false},
        {"-1", false},     {"4294967296", false}, {"true", false},
        {R"("12")", false}};

    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.first);
        ArduinoJson::DynamicJsonBuffer buffer{128};
        const auto value = tools::parseFullJson(testCase.first, buffer, 1);
        ASSERT_TRUE(value.success());
        uint32_t parsed = 91;
        EXPECT_EQ(tools::parseUint32(value, parsed), testCase.second);
        if (testCase.second) {
            EXPECT_EQ(
                parsed, static_cast<uint32_t>(std::stoull(testCase.first)));
        } else {
            EXPECT_EQ(parsed, 91u);
        }
    }
}
