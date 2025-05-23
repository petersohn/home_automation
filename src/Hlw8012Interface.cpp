#if 0

#include "Hlw8012Interface.hpp"
#include "common/ArduinoJson.hpp"
#include "tools/string.hpp"

using namespace ArduinoJson;

class Hlw8012Sensor : public Sensor {
public:
    Hlw8012Sensor(std::ostream& debug, EspApi& esp, HLW8012& hlw)
            : debug(debug), esp(esp), hlw(hlw) {}

    std::vector<std::string> measure() override {
        // return {};
        const auto now = esp.millis();
        const auto momentaryPower = hlw.getActivePower(); // W
        const auto energy = hlw.getEnergy(); // Ws
        const auto energyDiff = energy - previousEnergy;
        const auto avgPower = static_cast<double>(energyDiff) /
            (now - previousTime) * 1000.0;
        previousTime = now;
        previousEnergy = energy;
        return {
            tools::intToString(momentaryPower),
            tools::intToString(avgPower),
            tools::intToString(energyDiff),
            tools::intToString(energy),
        };
    }

private:
    std::ostream& debug;
    EspApi& esp;
    HLW8012& hlw;

    unsigned long previousEnergy = 0;
    unsigned long previousTime = 0;
};

Hlw8012Interface::Hlw8012Interface(std::ostream& debug, EspApi& esp,
        std::string name, int interval, int offset, uint8_t powerPin)
    : debug(debug), name(std::move(name)),
    sensor(std::make_unique<SensorInterface>(debug, esp,
        std::make_unique<Hlw8012Sensor>(debug, esp, hlw),
        this->name, interval, offset,
        std::vector<std::string>{})),
    jsonParser(debug) {
	esp.attachInterrupt(powerPin, [this]() { hlw.cf_interrupt(); }, InterruptMode::change);
    hlw.begin(powerPin, 0, 2);
    loadConfig();
}

void Hlw8012Interface::start() {
    sensor->start();
}

void Hlw8012Interface::update(Actions actions) {
    sensor->update(std::move(actions));
}

void Hlw8012Interface::execute(const std::string& command) {
    std::size_t position = 0;
    const auto cmd = tools::nextToken(command, ' ', position);
    if (cmd == "power") {
        const auto value = tools::nextToken(command, ' ', position);
        StaticJsonBuffer<10> buf;
        auto power = buf.parse(value);
        if (!power.is<unsigned long>()) {
            debug << "Invalid power value: " << value << "\n";
            return;
        }

        hlw.expectedActivePower(power);
        debug << name << ": new power multiplier=" << hlw.getPowerMultiplier()
            << "\n";
        saveConfig();
    } else {
        debug << "Invalid command: " << command << "\n";
    }
}

namespace {

constexpr const char* configFileName = "/hlw8012.json";

}

void Hlw8012Interface::loadConfig() {
    auto config = jsonParser.parseFile("/hlw8012.json");
    if (config.root == nullptr) {
        return;
    }

    auto& device = (*config.root)[name].as<JsonObject>();
    if (device == JsonObject::invalid()) {
        debug << name << ": no config for this device." << std::endl;
        return;
    }

    double powerMultiplier = 1.0;
    jsonParser.parseTo(device, powerMultiplier, "powerMultiplier");
    hlw.setPowerMultiplier(powerMultiplier);
    debug << name << ": power multiplier=" << powerMultiplier << "\n";
}

void Hlw8012Interface::saveConfig() {
    DynamicJsonBuffer buffer{100};
    auto& device = buffer.createObject();
    device["powerMultiplier"] = hlw.getPowerMultiplier();

    auto& root = buffer.createObject();
    root[name] = device;

    jsonParser.saveToFile(configFileName, root);
}

#endif
