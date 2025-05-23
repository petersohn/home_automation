#ifndef ECHO_DISTANCE_READER_INTERFACE_HPP
#define ECHO_DISTANCE_READER_INTERFACE_HPP

#include <ostream>

#include "common/EspApi.hpp"
#include "common/Interface.hpp"

class EchoDistanceReaderInterface : public Interface {
public:
    EchoDistanceReaderInterface(
        std::ostream& debug, EspApi& esp, uint8_t echoPin);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    std::ostream& debug;
    EspApi& esp;

    uint8_t echoPin;
    volatile unsigned long riseTime = 0;
    volatile unsigned long fallTime = 0;

    void onChange();
    static void onChangeStatic(void* arg);
    void reset();
};

#endif  // ECHO_DISTANCE_READER_INTERFACE_HPP
