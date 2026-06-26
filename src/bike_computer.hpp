#pragma once

#include <cstdint>

#include "sensors.hpp"

struct BikeComputerData
{
    sensors::DateTime time{};
    bool timeValid = false;
    float temperatureC = 0.0F;
    float pressureHpa = 0.0F;
    float altitudeM = 0.0F;
    bool barometerValid = false;
    int16_t accelRawX = 0;
    int16_t accelRawY = 0;
    int16_t accelRawZ = 0;
    float inclineDegrees = 0.0F;
    float motionG = 0.0F;
    bool imuValid = false;
    float batteryV = 0.0F;
    float speedKph = 0.0F;
    float distanceKm = 0.0F;
    float latitude = 0.0F;
    float longitude = 0.0F;
    float gpsAltitudeM = 0.0F;
    float courseDeg = 0.0F;
    int satellitesUsed = 0;
    int satellitesInView = 0;
    bool gpsHasData = false;
    bool gpsValid = false;
    bool gpsTimeValid = false;
};

void renderBikeComputer(uint8_t *framebuffer, const BikeComputerData &data);
