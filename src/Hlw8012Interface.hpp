#if 0

#ifndef HLW8012INTERFACE_HPP
#define HLW8012INTERFACE_HPP

#include "common/Interface.hpp"
#include "SensorInterface.hpp"
#include "common/EspApi.hpp"
#include "JsonParser.hpp"

#include <HLW8012.h>

#include <ostream>
#include <optional>

class Hlw8012Interface : public Interface {
public:
    Hlw8012Interface(std::ostream& debug, EspApi& esp, std::string name,
            int interval, int offset, uint8_t powerPin);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    std::ostream& debug;
    std::string name;
    HLW8012 hlw;
    std::unique_ptr<SensorInterface> sensor;
    JsonParser jsonParser;

    void loadConfig();
    void saveConfig();
};

#endif // HLW8012INTERFACE_HPP

#endif
