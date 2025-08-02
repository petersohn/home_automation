#include "EncoderInterface.hpp"

#include "common/Encoder.hpp"
#include "tools/string.hpp"

EncoderInterface::EncoderInterface(std::unique_ptr<Encoder> encoder, bool pulse)
    : encoder(std::move(encoder)), pulse(pulse) {}

void EncoderInterface::start() {}

void EncoderInterface::execute(const std::string& /*command*/) {}

void EncoderInterface::update(Actions action) {
    const int value = encoder->read();
    if (value == 0) {
        return;
    }

    action.fire({tools::intToString(value)});
    if (pulse) {
        action.fire({"0"});
    }
}
