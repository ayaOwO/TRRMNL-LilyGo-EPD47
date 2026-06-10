#pragma once

#include <cstdint>
#include <vector>

#include "driver/i2c.h"

extern "C" {
#include "bmi270.h"
}

namespace sensors
{
struct Ms5611
{
    uint16_t calibration[7]{};
};

struct DateTime
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    bool valid;
};

struct TouchPoint
{
    uint16_t x;
    uint16_t y;
    uint16_t size;
};

struct BarometerIdentity
{
    uint8_t boschChipId;
    uint8_t dps310ProductId;
    uint16_t ms5611Prom[7];
};

bool initI2c();
std::vector<uint8_t> scanI2cBus();
bool readBarometerIdentity(BarometerIdentity &identity);
bool readPcf8563(DateTime &dateTime);
bool setPcf8563(const DateTime &dateTime);
bool readGt911ProductId(char productId[5]);
bool readGt911Touches(std::vector<TouchPoint> &touches);
bool initMs5611(Ms5611 &sensor);
bool readMs5611(const Ms5611 &sensor, float &temperatureC, float &pressureHpa);
bool initBmi270(bmi2_dev &sensor, float &accelScale, float &gyroScale);
}
