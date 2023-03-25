#include "SDS011Sensor.hpp"
#include "tools/string.hpp"

SDS011Sensor::SDS011Sensor(std::ostream& debug, uint8_t rx, uint8_t tx,
        uint16_t measureTime)
    : debug(debug)
    , measureTime(measureTime)
    , isAwake(false)
    , failCount(0)
    , successCount(0)
{
    sds.begin(rx, tx);
    sds.sleep();
}

std::vector<std::string> SDS011Sensor::measure() {
    if (!isAwake) {
        sds.wakeup();
        isAwake = true;
        failCount = 0;
        successCount = 0;
    }

    float pm2_5 = 0.0, pm10 = 0.0;
    if (sds.read(&pm2_5, &pm10)) {
        successCount = 0;
        ++failCount;
        if (failCount >= 10) {
            debug << "Too many failures, trying to wake up again" << std::endl;
            isAwake = false;
        }

        return {};
    }

    ++successCount;
    failCount = 0;
    debug << "Measurement: " << pm2_5 << " " << pm10 << std::endl;

    if (successCount < measureTime) {
        return {};
    }

    sds.sleep();
    isAwake = false;

    return {tools::floatToString(pm2_5, 1), tools::floatToString(pm10, 1)};
}

