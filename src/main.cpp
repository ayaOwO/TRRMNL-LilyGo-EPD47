#include <Arduino.h>
#include <cmath>
#include <cstdio>

#include <epd_driver.h>

#include "battery.hpp"
#include "bike_computer.hpp"
#include "config.hpp"
#include "sensors.hpp"
#include "wifi_time.hpp"

namespace
{
constexpr float SEA_LEVEL_PRESSURE_HPA = 1013.25F;

uint8_t *framebuffer = nullptr;
sensors::Ms5611 barometer{};
bmi2_dev imu{};
float accelScale = 0.0F;
float gyroScale = 0.0F;
bool barometerReady = false;
bool imuReady = false;

BikeComputerData readBikeComputerData()
{
    BikeComputerData data{};
    data.timeValid = sensors::readPcf8563(data.time) && data.time.valid;

    if (barometerReady &&
        sensors::readMs5611(barometer, data.temperatureC, data.pressureHpa) &&
        data.temperatureC > -100.0F && data.temperatureC < 100.0F &&
        data.pressureHpa > 100.0F && data.pressureHpa < 1200.0F)
    {
        data.altitudeM =
            44307.694F *
            (1.0F - std::pow(data.pressureHpa / SEA_LEVEL_PRESSURE_HPA, 0.190284F));
        data.barometerValid = true;
    }

    if (imuReady)
    {
        bmi2_sens_data sample{};
        if (bmi2_get_sensor_data(&sample, &imu) == BMI2_OK)
        {
            const float x = sample.acc.x * accelScale;
            const float y = sample.acc.y * accelScale;
            const float z = sample.acc.z * accelScale;
            data.motionG = std::sqrt(x * x + y * y + z * z);
            data.inclineDegrees = std::atan2(x, std::sqrt(y * y + z * z)) * 180.0F / PI;
            data.imuValid = true;
        }
    }

    // Future UART GPS integration should update these four fields.
    data.batteryV = getBatteryVoltage();
    data.speedKph = 0.0F;
    data.distanceKm = 0.0F;
    data.gpsValid = false;
    return data;
}

void printData(const BikeComputerData &data)
{
    Serial.printf("RTC: %02u:%02u:%02u | GPS: %s | ",
                  data.time.hour, data.time.minute, data.time.second,
                  data.gpsValid ? "ready" : "waiting");
    Serial.printf("Altitude: %.1f m | Pressure: %.1f hPa | Temperature: %.1f C | ",
                  data.altitudeM, data.pressureHpa, data.temperatureC);
    Serial.printf("Incline: %.1f deg | Motion: %.2f g | Battery: %.2f V\n",
                  data.inclineDegrees, data.motionG, data.batteryV);
}
} // namespace

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("Bike computer starting...");

    if (!sensors::initI2c())
    {
        Serial.println("I2C initialization failed");
        while (true)
        {
            delay(1000);
        }
    }

#if RTC_MANUAL_SYNC_ENABLED
    const sensors::DateTime manualRtcTime{
        RTC_SYNC_YEAR, RTC_SYNC_MONTH, RTC_SYNC_DAY, RTC_SYNC_WEEKDAY,
        RTC_SYNC_HOUR, RTC_SYNC_MINUTE, RTC_SYNC_SECOND, true};
    Serial.printf("Manual RTC sync: %s\n",
                  sensors::setPcf8563(manualRtcTime) ? "complete" : "failed");
#endif

    syncRtcFromNtp();

    barometerReady = sensors::initMs5611(barometer);
    imuReady = sensors::initBmi270(imu, accelScale, gyroScale);
    Serial.printf("MS5611: %s | BMI270: %s\n",
                  barometerReady ? "ready" : "not found",
                  imuReady ? "ready" : "not found");

    epd_init();
    framebuffer = static_cast<uint8_t *>(
        heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM));
    if (framebuffer == nullptr)
    {
        Serial.println("Display framebuffer allocation failed");
        while (true)
        {
            delay(1000);
        }
    }
}

void loop()
{
    const BikeComputerData data = readBikeComputerData();
    printData(data);
    renderBikeComputer(framebuffer, data);
    delay(BIKE_COMPUTER_REFRESH_SECONDS * 1000UL);
}
