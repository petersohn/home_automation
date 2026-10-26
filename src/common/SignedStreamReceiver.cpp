#include "SignedStreamReceiver.hpp"

#include <utility>

#include "EspApi.hpp"

SignedStreamReceiver::SignedStreamReceiver(
    EspApi& esp, std::unique_ptr<SignatureVerifier> verifier,
    SignedStreamSink& sink, std::function<MqttTransportState()> transportState)
    : esp(esp)
    , verifier(std::move(verifier))
    , sink(sink)
    , transportState(std::move(transportState)) {}

void SignedStreamReceiver::execute(const std::string& command) {
    this->synchronizeTransport();
    if (!this->observedTransport.connected || this->busyPending) {
        return;
    }

    if (this->hasPendingCommand || this->oversizedPending) {
        this->pendingCommand.clear();
        this->hasPendingCommand = false;
        this->oversizedPending = false;
        this->busyPending = true;
        return;
    }

    if (command.size() > maxCommandSize) {
        this->oversizedPending = true;
        return;
    }

    this->pendingCommand = command;
    this->hasPendingCommand = true;
}

std::optional<SignedStreamReply> SignedStreamReceiver::update() {
    this->synchronizeTransport();

    if (this->disconnectedSnapshot.has_value()) {
        std::optional<SignedStreamReply> snapshot =
            std::move(this->disconnectedSnapshot);
        this->disconnectedSnapshot.reset();
        return snapshot;
    }

    if (this->busyPending) {
        this->busyPending = false;
        return this->errorReply(Error::busy);
    }

    if (this->oversizedPending) {
        this->oversizedPending = false;
        return this->errorReply(Error::commandTooLarge);
    }

    if (this->transferActive) {
        const uint32_t now = static_cast<uint32_t>(this->esp.millis());
        if (static_cast<uint32_t>(now - this->lastProgressTime) >=
            inactivityTimeoutMs) {
            this->pendingCommand.clear();
            this->hasPendingCommand = false;
            return this->errorReply(Error::timeout);
        }
    }

    if (this->hasPendingCommand) {
        std::string command = std::move(this->pendingCommand);
        this->pendingCommand.clear();
        this->hasPendingCommand = false;
        return this->processCommand(command);
    }

    return std::nullopt;
}

void SignedStreamReceiver::synchronizeTransport() {
    const MqttTransportState current = this->transportState();
    if (!this->transportInitialized) {
        this->observedTransport = current;
        this->transportInitialized = true;
        return;
    }

    const bool changed =
        current.generation != this->observedTransport.generation ||
        (this->observedTransport.connected && !current.connected);
    if (changed) {
        if (this->transferActive) {
            this->disconnectedSnapshot = transferReply(
                "ERROR disconnected", this->receivedBytes,
                streamHashText(this->transferHash));
            this->clearTransfer(true);
        }
        this->pendingCommand.clear();
        this->hasPendingCommand = false;
        this->oversizedPending = false;
        this->busyPending = false;
        this->terminalHashValid = false;
    }
    this->observedTransport = current;
}

void SignedStreamReceiver::clearTransfer(bool abortTransfer) {
    if (abortTransfer) {
        this->verifier->abort();
        this->sink.abort();
    }
    this->transferActive = false;
    this->transferHash.fill(0);
    this->transferSize = 0;
    this->receivedBytes = 0;
    this->lastProgressTime = 0;
}

std::optional<SignedStreamReply> SignedStreamReceiver::processCommand(
    const std::string& commandText) {
    StreamCommand command;
    const StreamCommandError decodeError =
        decodeStreamCommand(commandText, command);
    if (decodeError != StreamCommandError::none) {
        return this->errorReply(errorFor(decodeError));
    }

    const std::string hash = streamHashText(command.hash);
    switch (command.type) {
    case StreamCommandType::begin: {
        if (this->transferActive && command.hash == this->transferHash) {
            if (command.size != this->transferSize) {
                return this->errorReply(Error::conflictingSize, &command);
            }
            return transferReply(
                "PROGRESS", this->receivedBytes,
                streamHashText(this->transferHash));
        }

        if (this->transferActive) {
            this->clearTransfer(true);
        }
        this->terminalHashValid = false;

        const SignatureError beginError = this->verifier->begin();
        if (beginError != SignatureError::none) {
            this->verifier->abort();
            return this->signatureErrorReply(beginError, 0, hash);
        }
        if (!this->sink.begin(command.size)) {
            this->verifier->abort();
            this->sink.abort();
            return this->errorReply(Error::internalError, &command);
        }

        this->transferActive = true;
        this->transferHash = command.hash;
        this->transferSize = command.size;
        this->receivedBytes = 0;
        this->lastProgressTime = static_cast<uint32_t>(this->esp.millis());
        return transferReply("PROGRESS", 0, hash);
    }
    case StreamCommandType::chunk: {
        if (!this->transferActive) {
            return this->errorReply(Error::noActiveTransfer, &command);
        }
        if (command.hash != this->transferHash) {
            return this->errorReply(Error::transferMismatch, &command);
        }

        const StreamChunkPosition position = classifyStreamChunk(
            this->transferSize, this->receivedBytes, command.offset,
            command.dataSize);
        if (position == StreamChunkPosition::invalid) {
            return this->errorReply(Error::invalidOffset, &command);
        }
        if (position == StreamChunkPosition::duplicate) {
            return transferReply(
                "PROGRESS", this->receivedBytes,
                streamHashText(this->transferHash));
        }

        const SignatureBytes data{command.data.data(), command.dataSize};
        const SignatureError appendError = this->verifier->append(data);
        if (appendError != SignatureError::none) {
            const SignedStreamReply result = this->signatureErrorReply(
                appendError, this->receivedBytes,
                streamHashText(this->transferHash));
            this->clearTransfer(true);
            return result;
        }
        if (!this->sink.append(data)) {
            return this->errorReply(Error::internalError, &command);
        }

        this->receivedBytes += static_cast<uint32_t>(command.dataSize);
        this->lastProgressTime = static_cast<uint32_t>(this->esp.millis());
        return transferReply(
            "PROGRESS", this->receivedBytes,
            streamHashText(this->transferHash));
    }
    case StreamCommandType::finish: {
        if (!this->transferActive) {
            if (this->terminalHashValid && command.hash == this->terminalHash) {
                return std::nullopt;
            }
            return this->errorReply(Error::noActiveTransfer, &command);
        }
        if (command.hash != this->transferHash) {
            return this->errorReply(Error::transferMismatch, &command);
        }
        if (this->receivedBytes != this->transferSize) {
            return this->errorReply(Error::incompleteTransfer, &command);
        }

        const uint32_t count = this->receivedBytes;
        const std::string activeHash = streamHashText(this->transferHash);
        const SignatureResult result = this->verifier->finish(
            SignatureBytes{command.hash.data(), command.hash.size()},
            SignatureBytes{command.signature.data(), command.signature.size()});
        this->terminalHash = this->transferHash;
        this->terminalHashValid = true;
        this->clearTransfer(false);

        if (result.status == SignatureStatus::valid &&
            result.error == SignatureError::none) {
            const std::string status =
                this->sink.complete(transferReply("OK", count, activeHash));
            if (status != "OK" && status != "STAGED") {
                this->sink.abort();
            }
            return transferReply(status, count, activeHash);
        }

        this->sink.abort();
        if (result.status == SignatureStatus::invalid ||
            result.status == SignatureStatus::error) {
            return this->signatureErrorReply(
                result.error == SignatureError::none
                    ? SignatureError::internalError
                    : result.error,
                count, activeHash);
        }
        return this->signatureErrorReply(
            SignatureError::internalError, count, activeHash);
    }
    }
    return this->errorReply(Error::internalError, &command);
}

SignedStreamReply SignedStreamReceiver::errorReply(
    Error error, const StreamCommand* command) {
    uint32_t count = 0;
    std::string hash;
    if (this->transferActive) {
        count = this->receivedBytes;
        hash = streamHashText(this->transferHash);
        this->clearTransfer(true);
    } else if (command != nullptr) {
        hash = streamHashText(command->hash);
    }
    return transferReply(
        std::string("ERROR ") + errorMessage(error), count, hash);
}

SignedStreamReply SignedStreamReceiver::signatureErrorReply(
    SignatureError error, uint32_t count, const std::string& hash) {
    if (error == SignatureError::hashMismatch) {
        return transferReply("FAIL hash mismatch", count, hash);
    }
    if (error == SignatureError::invalidSignature) {
        return transferReply("FAIL invalid signature", count, hash);
    }
    return transferReply(
        std::string("ERROR ") + errorMessage(errorFor(error)), count, hash);
}

const char* SignedStreamReceiver::errorMessage(Error error) {
    switch (error) {
    case Error::invalidJson:
        return "invalid JSON";
    case Error::invalidFields:
        return "invalid fields";
    case Error::invalidHash:
        return "invalid hash";
    case Error::invalidBase64:
        return "invalid base64";
    case Error::commandTooLarge:
        return "command too large";
    case Error::dataTooLarge:
        return "data too large";
    case Error::invalidSignatureLength:
        return "invalid signature length";
    case Error::busy:
        return "busy";
    case Error::conflictingSize:
        return "conflicting size";
    case Error::invalidOffset:
        return "invalid offset";
    case Error::transferMismatch:
        return "transfer mismatch";
    case Error::noActiveTransfer:
        return "no active transfer";
    case Error::incompleteTransfer:
        return "incomplete transfer";
    case Error::timeout:
        return "timeout";
    case Error::disconnected:
        return "disconnected";
    case Error::invalidCertificate:
        return "invalid certificate";
    case Error::unsupportedKey:
        return "unsupported key";
    case Error::certificateUnavailable:
        return "certificate unavailable";
    case Error::invalidState:
        return "invalid state";
    case Error::internalError:
        return "internal error";
    }
    return "internal error";
}

SignedStreamReceiver::Error SignedStreamReceiver::errorFor(
    StreamCommandError error) {
    switch (error) {
    case StreamCommandError::invalidJson:
        return Error::invalidJson;
    case StreamCommandError::invalidFields:
        return Error::invalidFields;
    case StreamCommandError::invalidHash:
        return Error::invalidHash;
    case StreamCommandError::invalidBase64:
        return Error::invalidBase64;
    case StreamCommandError::commandTooLarge:
        return Error::commandTooLarge;
    case StreamCommandError::dataTooLarge:
        return Error::dataTooLarge;
    case StreamCommandError::invalidSignatureLength:
        return Error::invalidSignatureLength;
    case StreamCommandError::none:
        break;
    }
    return Error::internalError;
}

SignedStreamReceiver::Error SignedStreamReceiver::errorFor(
    SignatureError error) {
    switch (error) {
    case SignatureError::none:
        return Error::internalError;
    case SignatureError::invalidCertificate:
        return Error::invalidCertificate;
    case SignatureError::unsupportedKey:
        return Error::unsupportedKey;
    case SignatureError::certificateUnavailable:
        return Error::certificateUnavailable;
    case SignatureError::invalidSignatureLength:
        return Error::invalidSignatureLength;
    case SignatureError::hashMismatch:
        return Error::internalError;
    case SignatureError::invalidState:
        return Error::invalidState;
    case SignatureError::invalidSignature:
    case SignatureError::internalError:
        return Error::internalError;
    }
    return Error::internalError;
}

SignedStreamReply SignedStreamReceiver::transferReply(
    const std::string& status, uint32_t count, const std::string& hash) {
    return {status, count, hash};
}
