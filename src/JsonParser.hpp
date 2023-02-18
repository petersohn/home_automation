#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include "common/ArduinoJson.hpp"

#include <memory>
#include <ostream>

struct ParsedData {
    std::unique_ptr<ArduinoJson::DynamicJsonBuffer> buffer =
        std::make_unique<ArduinoJson::DynamicJsonBuffer>(512);
    ArduinoJson::JsonObject* root = nullptr;
};

class JsonParser {
public:
    JsonParser(std::ostream& debug) : debug(debug) {}

    template<typename T>
    void parseTo(const ArduinoJson::JsonObject& jsonObject, T& target,
            const char* name) {
        auto value = jsonObject.get<ArduinoJson::JsonVariant>(name);
        if (value.is<T>()) {
            target = value.as<T>();
        }
    }

    void parseTo(const ArduinoJson::JsonObject& jsonObject,
            std::string& target, const char* name) {
        auto value = jsonObject.get<const char*>(name);
        if (value) {
            target = value;
        }
    }

    ParsedData parseFile(const char* filename);
    void saveToFile(const char* filename, const ArduinoJson::JsonObject& obj);

private:
    std::ostream& debug;
};

#define PARSE(obj, from, to, name) (obj).parseTo((from), (to).name, #name)

#endif // CONFIGPARSER_HPP
