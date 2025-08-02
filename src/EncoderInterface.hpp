#ifndef ENCODER_INTERFACE_HPP
#define ENCODER_INTERFACE_HPP

#include "common/Interface.hpp"

class Encoder;

class EncoderInterface : public Interface {
public:
    EncoderInterface(std::unique_ptr<Encoder> encoder, bool pulse);
    virtual void start() override;
    virtual void execute(const std::string& command) override;
    virtual void update(Actions action) override;

private:
    const std::unique_ptr<Encoder> encoder;
    const bool pulse;
};

#endif  // ENCODER_INTERFACE_HPP
