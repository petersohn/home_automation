#include "SignatureVerifyInterface.hpp"

#include <utility>

#include "EspApi.hpp"

SignatureVerifyInterface::SignatureVerifyInterface(
    EspApi& esp, std::unique_ptr<SignatureVerifier> verifier,
    std::function<MqttTransportState()> transportState)
    : esp(esp)
    , verifier(std::move(verifier))
    , transportState(std::move(transportState)) {}

void SignatureVerifyInterface::start() {}

void SignatureVerifyInterface::execute(const std::string& command) {
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

void SignatureVerifyInterface::update(Actions action) {
    this->synchronizeTransport();

    if (this->initialEventPending) {
        this->initialEventPending = false;
        action.fire(std::vector<std::string>{""});
        return;
    }

    if (!this->disconnectedSnapshot.empty()) {
        action.fire(std::move(this->disconnectedSnapshot));
        this->disconnectedSnapshot.clear();
        return;
    }

    if (this->busyPending) {
        this->busyPending = false;
        action.fire(this->errorReply(Error::busy));
        return;
    }

    if (this->oversizedPending) {
        this->oversizedPending = false;
        action.fire(this->errorReply(Error::commandTooLarge));
        return;
    }

    if (this->transferActive) {
        const uint32_t now = static_cast<uint32_t>(this->esp.millis());
        if (static_cast<uint32_t>(now - this->lastProgressTime) >=
            inactivityTimeoutMs) {
            this->pendingCommand.clear();
            this->hasPendingCommand = false;
            action.fire(this->errorReply(Error::timeout));
            return;
        }
    }

    if (this->hasPendingCommand) {
        std::string command = std::move(this->pendingCommand);
        this->pendingCommand.clear();
        this->hasPendingCommand = false;
        std::vector<std::string> result = this->processCommand(command);
        if (!result.empty()) {
            action.fire(result);
        }
    }
}

void SignatureVerifyInterface::synchronizeTransport() {
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

void SignatureVerifyInterface::clearTransfer(bool abortVerifier) {
    if (abortVerifier) {
        this->verifier->abort();
    }
    this->transferActive = false;
    this->transferHash.fill(0);
    this->transferSize = 0;
    this->receivedBytes = 0;
    this->lastProgressTime = 0;
}

std::vector<std::string> SignatureVerifyInterface::processCommand(
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
            this->verifier->abort();
            this->clearTransfer(false);
        }
        this->terminalHashValid = false;

        const SignatureError beginError = this->verifier->begin();
        if (beginError != SignatureError::none) {
            this->verifier->abort();
            return this->signatureErrorReply(beginError, 0, hash);
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

        const SignatureError appendError = this->verifier->append(
            SignatureBytes{command.data.data(), command.dataSize});
        if (appendError != SignatureError::none) {
            const std::vector<std::string> result = this->signatureErrorReply(
                appendError, this->receivedBytes,
                streamHashText(this->transferHash));
            this->clearTransfer(true);
            return result;
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
                return {};
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
            return transferReply("OK", count, activeHash);
        }
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

std::vector<std::string> SignatureVerifyInterface::errorReply(
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

std::vector<std::string> SignatureVerifyInterface::signatureErrorReply(
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

const char* SignatureVerifyInterface::errorMessage(Error error) {
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

SignatureVerifyInterface::Error SignatureVerifyInterface::errorFor(
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

SignatureVerifyInterface::Error SignatureVerifyInterface::errorFor(
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

std::vector<std::string> SignatureVerifyInterface::transferReply(
    const std::string& status, uint32_t count, const std::string& hash) {
    return {status, std::to_string(count), hash};
}
