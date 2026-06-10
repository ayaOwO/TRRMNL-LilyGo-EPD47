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
    float inclineDegrees = 0.0F;
    float motionG = 0.0F;
    bool imuValid = false;
    float batteryV = 0.0F;
    float speedKph = 0.0F;
    float distanceKm = 0.0F;
    bool gpsValid = false;
};

void renderBikeComputer(uint8_t *framebuffer, const BikeComputerData &data);
