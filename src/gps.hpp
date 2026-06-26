#pragma once

#include "sensors.hpp"

namespace gps
{
struct Data
{
    bool hasData = false;
    bool hasFix = false;
    bool hasCoords = false;
    bool hasTime = false;
    int fixQuality = 0;
    int fixMode = 1;
    int satellitesUsed = 0;
    int satellitesInView = 0;
    float latitude = 0.0F;
    float longitude = 0.0F;
    float altitudeM = 0.0F;
    float speedKph = 0.0F;
    float courseDeg = 0.0F;
    float hdop = 0.0F;
    sensors::DateTime utcTime{};
};

void begin();
void update();
Data read();
}
