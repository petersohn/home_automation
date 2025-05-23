#include <FS.h>

#include "JsonParser.hpp"

using namespace ArduinoJson;

ParsedData JsonParser::parseFile(const char* filename) {
    ParsedData result;
    File f = SPIFFS.open(filename, "r");
    if (!f) {
        debug << "Could not open file: " << filename << std::endl;
        return result;
    }

    result.root = &result.buffer->parseObject(f);
    if (*result.root == JsonObject::invalid()) {
        debug << "Could not parse JSON file: " << filename << std::endl;
        result.root = nullptr;
    }

    return result;
}

void JsonParser::saveToFile(
    const char* filename, const ArduinoJson::JsonObject& obj) {
    File f = SPIFFS.open(filename, "w");
    if (!f) {
        debug << "Failed to create file: " << filename << std::endl;
        return;
    }

    obj.printTo(f);
    f.close();
}
