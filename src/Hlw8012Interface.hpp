#if 0

#ifndef HLW8012INTERFACE_HPP
#define HLW8012INTERFACE_HPP

#include <HLW8012.h>

#include <optional>
#include <ostream>

#include "JsonParser.hpp"
#include "common/EspApi.hpp"
#include "common/Interface.hpp"
#include "common/SensorInterface.hpp"

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

#endif  // HLW8012INTERFACE_HPP

#endif
