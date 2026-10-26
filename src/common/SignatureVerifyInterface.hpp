#ifndef COMMON_SIGNATUREVERIFYINTERFACE_HPP
#define COMMON_SIGNATUREVERIFYINTERFACE_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Interface.hpp"
#include "MqttTransportState.hpp"
#include "SignatureVerifier.hpp"

class SignedStreamReceiver;
class SignedStreamSink;

class EspApi;

class SignatureVerifyInterface : public Interface {
public:
    SignatureVerifyInterface(
        EspApi& esp, std::unique_ptr<SignatureVerifier> verifier,
        std::function<MqttTransportState()> transportState);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

    ~SignatureVerifyInterface() override;

private:
    bool initialEventPending = true;
    std::unique_ptr<SignedStreamSink> sink;
    std::unique_ptr<SignedStreamReceiver> receiver;
};

#endif  // COMMON_SIGNATUREVERIFYINTERFACE_HPP
