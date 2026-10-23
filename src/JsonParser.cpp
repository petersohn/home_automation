#include "JsonParser.hpp"

#include <FS.h>

#include "tools/JsonParsing.hpp"

using namespace ArduinoJson;

ParsedData JsonParser::parseFile(const char* filename) {
    ParsedData result;
    File f = SPIFFS.open(filename, "r");
    if (!f) {
        this->debug << "Could not open file: " << filename << std::endl;
        return result;
    }

    const JsonVariant parsed = tools::parseFullJson(
        f, *result.buffer, ARDUINOJSON_DEFAULT_NESTING_LIMIT);
    if (!parsed.success() || !parsed.is<JsonObject>()) {
        this->debug << "Could not parse JSON file: " << filename << std::endl;
        return result;
    }
    result.root = &parsed.as<JsonObject>();

    return result;
}

void JsonParser::saveToFile(
    const char* filename, const ArduinoJson::JsonObject& obj) {
    File f = SPIFFS.open(filename, "w");
    if (!f) {
        this->debug << "Failed to create file: " << filename << std::endl;
        return;
    }

    obj.printTo(f);
    f.close();
}
