#include "SignatureVerifyInterface.hpp"

#include <optional>
#include <utility>
#include <vector>

#include "SignedStreamReceiver.hpp"

namespace {

class DiscardSignedStreamSink : public SignedStreamSink {
public:
    bool begin(uint32_t) override { return true; }

    bool append(SignatureBytes) override { return true; }

    std::string complete(const SignedStreamReply&) override { return "OK"; }

    void abort() override {}
};

std::vector<std::string> asVector(const SignedStreamReply& reply) {
    return {reply.status, std::to_string(reply.transferred), reply.hash};
}

}  // namespace

SignatureVerifyInterface::SignatureVerifyInterface(
    EspApi& esp, std::unique_ptr<SignatureVerifier> verifier,
    std::function<MqttTransportState()> transportState)
    : sink(std::make_unique<DiscardSignedStreamSink>())
    , receiver(
          std::make_unique<SignedStreamReceiver>(
              esp, std::move(verifier), *this->sink,
              std::move(transportState))) {}

SignatureVerifyInterface::~SignatureVerifyInterface() = default;

void SignatureVerifyInterface::start() {}

void SignatureVerifyInterface::execute(const std::string& command) {
    this->receiver->execute(command);
}

void SignatureVerifyInterface::update(Actions action) {
    if (this->initialEventPending) {
        this->initialEventPending = false;
        action.fire(std::vector<std::string>{""});
        return;
    }

    const std::optional<SignedStreamReply> reply = this->receiver->update();
    if (reply.has_value()) {
        action.fire(asVector(*reply));
    }
}
