#include "DeviceConfigOta.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "ConfigBootCoordinator.hpp"
#include "EspApi.hpp"
#include "MqttClient.hpp"
#include "OtaDocument.hpp"
#include "Sha256.hpp"
#include "UploadStatus.hpp"

namespace {

constexpr std::size_t maxConfigBytes = 32768;
constexpr std::size_t maxWriterChunk = 256;

std::string streamFailure(const char* prefix, std::string status) {
    const std::string expected(prefix);
    if (status.compare(0, expected.size(), expected) != 0) {
        return status;
    }
    return status.substr(expected.size());
}

class BoundedFileWriter {
public:
    explicit BoundedFileWriter(OtaFile& file) : file(file) {}

    std::size_t print(char value) {
        return this->append(static_cast<uint8_t>(value)) ? 1 : 0;
    }

    std::size_t print(const char* value) {
        std::size_t written = 0;
        while (value != nullptr && *value != '\0' &&
               this->append(static_cast<uint8_t>(*value))) {
            ++value;
            ++written;
        }
        return written;
    }

    bool flush() {
        if (this->failed) {
            return false;
        }
        if (this->buffered == 0) {
            return true;
        }
        const SignatureBytes bytes{this->buffer.data(), this->buffered};
        const std::size_t written = this->file.write(bytes);
        if (written != this->buffered || this->file.failed()) {
            this->failed = true;
            return false;
        }
        this->buffered = 0;
        return true;
    }

    bool failedWriting() const { return this->failed; }
    std::size_t bytesWritten() const { return this->totalBytes; }

private:
    bool append(uint8_t value) {
        if (this->failed || this->totalBytes == maxConfigBytes) {
            this->failed = true;
            return false;
        }
        this->buffer[this->buffered++] = value;
        ++this->totalBytes;
        return this->buffered != this->buffer.size() || this->flush();
    }

    OtaFile& file;
    std::array<uint8_t, maxWriterChunk> buffer{};
    std::size_t buffered = 0;
    std::size_t totalBytes = 0;
    bool failed = false;
};

class BoundedFileComparator {
public:
    explicit BoundedFileComparator(OtaFile& file) : file(file) {}

    std::size_t print(char value) {
        if (this->bytesCompared == maxConfigBytes) {
            this->tooLarge = true;
            return 0;
        }
        ++this->bytesCompared;
        if (this->readFailed || this->atEof) {
            this->matches = false;
            return 1;
        }

        const int actual = this->file.read();
        if (actual == -1) {
            this->atEof = true;
            this->matches = false;
        } else if (actual < 0 || actual > 255 || this->file.failed()) {
            this->readFailed = true;
        } else if (
            static_cast<uint8_t>(actual) != static_cast<uint8_t>(value)) {
            this->matches = false;
        }
        return 1;
    }

    std::size_t print(const char* value) {
        std::size_t compared = 0;
        while (value != nullptr && *value != '\0') {
            if (this->print(*value) == 0) {
                break;
            }
            ++value;
            ++compared;
        }
        return compared;
    }

    bool finish() {
        if (this->tooLarge || this->bytesCompared == 0) {
            return false;
        }
        if (!this->readFailed && !this->atEof) {
            const int trailing = this->file.read();
            if (trailing == -1) {
                this->atEof = true;
            } else if (trailing < 0 || trailing > 255 || this->file.failed()) {
                this->readFailed = true;
            } else {
                this->matches = false;
            }
        }
        return this->matches && this->atEof && !this->readFailed &&
               !this->file.failed();
    }

    bool failed() const { return this->readFailed || this->file.failed(); }

private:
    OtaFile& file;
    std::size_t bytesCompared = 0;
    bool matches = true;
    bool atEof = false;
    bool readFailed = false;
    bool tooLarge = false;
};

class HashingOtaFile final : public OtaFile {
public:
    explicit HashingOtaFile(OtaFile& file) : file(file) {}

    int read() override {
        const int value = this->file.read();
        if (value >= 0 && value <= 255) {
            this->hash.update(static_cast<uint8_t>(value));
            ++this->bytesRead;
        }
        return value;
    }

    std::size_t write(SignatureBytes bytes) override {
        return this->file.write(bytes);
    }

    bool close() override { return this->file.close(); }

    bool failed() const override { return this->file.failed(); }

    bool drain(uint32_t expectedBytes) {
        while (this->bytesRead <= expectedBytes) {
            const int value = this->file.read();
            if (value == -1) {
                return !this->file.failed();
            }
            if (value < 0 || value > 255 || this->file.failed()) {
                return false;
            }
            this->hash.update(static_cast<uint8_t>(value));
            ++this->bytesRead;
            if (this->bytesRead > expectedBytes) {
                return true;
            }
        }
        return true;
    }

    bool matches(const SignedStreamReply& verified) {
        const std::array<uint8_t, 32> digest = this->hash.finish();
        if (this->bytesRead != verified.transferred ||
            verified.hash.size() != 64) {
            return false;
        }
        static constexpr char hexDigits[] = "0123456789abcdef";
        for (std::size_t index = 0; index < digest.size(); ++index) {
            if (verified.hash[index * 2] != hexDigits[digest[index] >> 4] ||
                verified.hash[index * 2 + 1] !=
                    hexDigits[digest[index] & 0x0F]) {
                return false;
            }
        }
        return true;
    }

private:
    OtaFile& file;
    Sha256 hash;
    std::size_t bytesRead = 0;
};

bool validateConfigReadback(
    OtaFileSystem& fileSystem, const char* path,
    const ArduinoJson::JsonObject& config) {
    std::unique_ptr<OtaFile> input = fileSystem.open(path, OtaFileMode::read);
    if (input == nullptr) {
        return false;
    }

    BoundedFileComparator comparator(*input);
    config.printTo(comparator);
    const bool matches = comparator.finish();
    bool readFailed = comparator.failed() || input->failed();
    const bool closeSucceeded = input->close();
    readFailed = readFailed || input->failed();
    return matches && !readFailed && closeSucceeded;
}

bool writeAndVerifyConfig(
    OtaFileSystem& fileSystem, ConfigSlot slot,
    const ArduinoJson::JsonObject& config) {
    const char* const path = configSlotPath(slot);
    if (path == nullptr) {
        return false;
    }

    std::unique_ptr<OtaFile> output =
        fileSystem.open(path, OtaFileMode::truncate);
    if (output == nullptr) {
        return false;
    }

    BoundedFileWriter writer(*output);
    config.printTo(writer);
    const bool writeSucceeded =
        writer.flush() && !writer.failedWriting() && writer.bytesWritten() != 0;
    const bool closeSucceeded = output->close();
    const bool closeFailed = output->failed();
    if (!writeSucceeded || !closeSucceeded || closeFailed) {
        return false;
    }

    return validateConfigReadback(fileSystem, path, config);
}

OtaResult lifecycleResult(const OtaStatusSnapshot& snapshot) {
    return snapshot.lastOutcome.has_value() ? *snapshot.lastOutcome
                                            : OtaResult{};
}

}  // namespace

DeviceConfigOta::DeviceConfigOta(
    EspApi& esp, OtaFileSystem& fileSystem,
    ConfigBootCoordinator& bootCoordinator,
    std::unique_ptr<SignatureVerifier> verifier, std::string localMac,
    std::function<MqttTransportState()> transportState,
    std::function<void(const OtaResult&)> resultCallback)
    : esp(esp)
    , fileSystem(fileSystem)
    , bootCoordinator(bootCoordinator)
    , localMac(std::move(localMac))
    , resultCallback(std::move(resultCallback))
    , receiver(esp, std::move(verifier), *this, std::move(transportState))
    , readiness(MqttReadiness::checking) {
    const OtaStatusSnapshot snapshot = this->bootCoordinator.snapshot();
    if (snapshot.lastOutcome.has_value()) {
        this->startupOutcome = *snapshot.lastOutcome;
    }
}

void DeviceConfigOta::execute(const std::string& command, bool retained) {
    if (this->storageFaulted) {
        return;
    }

    const OtaStatusSnapshot snapshot = this->bootCoordinator.snapshot();
    if (retained || this->restartPending ||
        this->bootCoordinator.trialRunning() ||
        snapshot.phase != UploadPhase::idle || !snapshot.updatesAllowed) {
        if (!this->deferredResult.has_value()) {
            OtaResult result;
            result.status = "ERROR";
            if (retained) {
                result.reason = "retained-command";
            } else if (!snapshot.minimumAcceptedConfigVersion.has_value()) {
                result.reason = "version-range-exhausted";
                if (command.size() <= 512) {
                    StreamCommand decoded;
                    if (decodeStreamCommand(command, decoded) ==
                            StreamCommandError::none &&
                        decoded.type == StreamCommandType::begin) {
                        result.hash = streamHashText(decoded.hash);
                    }
                }
            } else {
                result.reason = "updates-disabled";
            }
            this->deferredResult = std::move(result);
        }
        return;
    }

    this->receiver.execute(command);
}

void DeviceConfigOta::update() {
    if (this->restartPending) {
        this->restartPending = false;
        this->esp.restart(false);
        return;
    }

    if (this->startupOutcome.has_value()) {
        this->publishResult(*this->startupOutcome);
        this->startupOutcome.reset();
    }
    if (this->deferredResult.has_value()) {
        this->publishResult(*this->deferredResult);
        this->deferredResult.reset();
    }

    if (this->processTrialSignal()) {
        return;
    }

    if (this->storageFaulted) {
        return;
    }

    const std::optional<SignedStreamReply> reply = this->receiver.update();
    if (reply.has_value()) {
        this->processReply(*reply);
    }
}

void DeviceConfigOta::observeMqtt(MqttReadiness readiness, bool collided) {
    this->readiness = readiness;
    this->collisionPending = this->collisionPending || collided ||
                             readiness == MqttReadiness::collision;
}

bool DeviceConfigOta::begin(uint32_t size) {
    this->oversizedBeginRejected = false;
    this->preserveDownloadOnAbort = false;
    if (size > maxConfigBytes) {
        this->oversizedBeginRejected = true;
        return false;
    }
    if (this->storageFaulted) {
        return false;
    }

    const OtaStatusSnapshot snapshot = this->bootCoordinator.snapshot();
    if (snapshot.phase != UploadPhase::idle || !snapshot.updatesAllowed ||
        this->bootCoordinator.trialRunning()) {
        return false;
    }

    this->downloadFile =
        this->fileSystem.open(downloadPath, OtaFileMode::truncate);
    if (this->downloadFile == nullptr) {
        this->storageFaulted = true;
        return false;
    }
    return true;
}

bool DeviceConfigOta::append(SignatureBytes data) {
    if (this->storageFaulted || this->downloadFile == nullptr) {
        return false;
    }

    const size_t written = this->downloadFile->write(data);
    if (written != data.size || this->downloadFile->failed()) {
        this->storageFaulted = true;
        return false;
    }
    return true;
}

std::string DeviceConfigOta::complete(const SignedStreamReply& verified) {
    if (this->storageFaulted) {
        return "ERROR storage-fault";
    }

    if (verified.status != "OK" || !this->closeDownload()) {
        return "ERROR envelope-close-failed";
    }

    uint32_t version = 0;
    const ConfigSlot candidateSlot = this->bootCoordinator.inactiveSlot();
    {
        std::unique_ptr<OtaFile> input =
            this->fileSystem.open(downloadPath, OtaFileMode::read);
        if (input == nullptr) {
            this->storageFaulted = true;
            return "ERROR envelope-open-failed";
        }

        HashingOtaFile authenticatedInput(*input);
        ParsedOtaDocument parsed = parseOtaDocument(
            authenticatedInput, this->localMac,
            this->bootCoordinator.snapshot().minimumAcceptedConfigVersion);
        const bool drainSucceeded =
            authenticatedInput.drain(verified.transferred);
        const bool readbackMatches = authenticatedInput.matches(verified);
        const bool closeSucceeded = authenticatedInput.close();
        const bool closeFailed = input->failed();
        if (!drainSucceeded || parsed.policy.reason == "file-read-error") {
            this->storageFaulted = true;
            return "ERROR envelope-read-failed";
        }
        if (!closeSucceeded || closeFailed) {
            this->storageFaulted = true;
            return "ERROR envelope-close-failed";
        }
        if (!readbackMatches) {
            this->storageFaulted = true;
            this->preserveDownloadOnAbort = true;
            return "ERROR envelope-readback-mismatch";
        }
        if (!parsed.policy.valid) {
            return std::string("ERROR ") + parsed.policy.reason;
        }
        if (parsed.config == nullptr) {
            return "ERROR invalid-config";
        }

        version = parsed.policy.version;
        if (!writeAndVerifyConfig(
                this->fileSystem, candidateSlot, *parsed.config)) {
            this->storageFaulted = true;
            return "ERROR candidate-write-failed";
        }
    }

    if (!this->fileSystem.remove(downloadPath)) {
        this->storageFaulted = true;
        return "ERROR envelope-remove-failed";
    }
    if (!this->bootCoordinator.stage(
            candidateSlot, version, verified.hash, verified.transferred)) {
        this->storageFaulted = true;
        return "ERROR stage-failed";
    }

    return "STAGED";
}

void DeviceConfigOta::abort() {
    (void)this->closeDownload();
    if (this->preserveDownloadOnAbort) {
        return;
    }
    if (!this->fileSystem.remove(downloadPath)) {
        this->storageFaulted = true;
    }
}

bool DeviceConfigOta::closeDownload() {
    if (this->downloadFile == nullptr) {
        return true;
    }

    const bool closeSucceeded = this->downloadFile->close();
    const bool closeFailed = this->downloadFile->failed();
    this->downloadFile.reset();
    if (!closeSucceeded || closeFailed) {
        this->storageFaulted = true;
        return false;
    }
    return true;
}

void DeviceConfigOta::processReply(const SignedStreamReply& reply) {
    OtaResult result;
    result.transferred = reply.transferred;
    result.hash = reply.hash;

    if (reply.status.compare(0, 5, "FAIL ") == 0) {
        result.status = "FAIL";
        result.reason = streamFailure("FAIL ", reply.status);
    } else if (reply.status.compare(0, 6, "ERROR ") == 0) {
        result.status = "ERROR";
        result.reason = streamFailure("ERROR ", reply.status);
        if (this->oversizedBeginRejected) {
            result.reason = "document-too-large";
        }
    } else if (reply.status == "STAGED") {
        result = lifecycleResult(this->bootCoordinator.snapshot());
        this->restartPending = result.status == "STAGED";
    } else if (reply.status == "PROGRESS") {
        result.status = "PROGRESS";
    } else {
        result.status = "ERROR";
        result.reason = "internal error";
    }
    this->oversizedBeginRejected = false;

    this->publishResult(result);
}

void DeviceConfigOta::publishResult(const OtaResult& result) {
    if (!encodeOtaResult(result).empty() && this->resultCallback) {
        this->resultCallback(result);
    }
}

bool DeviceConfigOta::processTrialSignal() {
    if (!this->bootCoordinator.trialRunning()) {
        this->collisionPending = false;
        return false;
    }

    if (this->collisionPending) {
        this->collisionPending = false;
        if (this->bootCoordinator.rollback("candidate-collision")) {
            const OtaStatusSnapshot snapshot = this->bootCoordinator.snapshot();
            if (snapshot.lastOutcome.has_value()) {
                this->publishResult(*snapshot.lastOutcome);
            }
        }
        this->esp.restart(false);
        return true;
    }

    if (this->readiness == MqttReadiness::ready) {
        if (this->bootCoordinator.confirm()) {
            const OtaStatusSnapshot snapshot = this->bootCoordinator.snapshot();
            if (snapshot.lastOutcome.has_value()) {
                this->publishResult(*snapshot.lastOutcome);
            }
        }
        return true;
    }

    return false;
}
