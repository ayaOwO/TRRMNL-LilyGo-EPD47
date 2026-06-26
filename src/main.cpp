#include <Arduino.h>
#include <cmath>
#include <cstdio>

#include <epd_driver.h>

#include "battery.hpp"
#include "bike_computer.hpp"
#include "config.hpp"
#include "gps.hpp"
#include "sensors.hpp"

namespace
{
constexpr float SEA_LEVEL_PRESSURE_HPA = 1013.25F;
constexpr uint32_t MILLISECONDS_PER_SECOND = 1000UL;
constexpr uint32_t GPS_POLL_DELAY_MS = 5;

uint8_t *framebuffer = nullptr;
sensors::Ms5611 barometer{};
bmi2_dev imu{};
float accelScale = 0.0F;
float gyroScale = 0.0F;
bool barometerReady = false;
bool imuReady = false;
bool rtcSyncedFromGps = false;

void logMs5611Calibration()
{
    for (uint8_t index = 0; index < 7; ++index)
    {
        Serial.printf("MS5611 calibration[%u]=0x%04X (%u)\n",
                      index, barometer.calibration[index], barometer.calibration[index]);
    }
}

void tryInitBarometer()
{
    Serial.println("Initializing MS5611 barometer...");
    barometerReady = sensors::initMs5611(barometer);
    Serial.printf("MS5611 init result: %s\n", barometerReady ? "ready" : "failed");
    logMs5611Calibration();
}

void tryInitImu()
{
    Serial.println("Initializing BMI270 IMU...");
    imuReady = sensors::initBmi270(imu, accelScale, gyroScale);
    Serial.printf("BMI270 init result: %s\n", imuReady ? "ready" : "failed");
}

void logI2cScan()
{
    const std::vector<uint8_t> devices = sensors::scanI2cBus();
    Serial.printf("I2C scan: found %u device(s)", static_cast<unsigned>(devices.size()));
    for (const uint8_t address : devices)
    {
        Serial.printf(" 0x%02X", address);
    }
    Serial.println();
}

void logBarometerIdentity()
{
    sensors::BarometerIdentity identity{};
    if (!sensors::readBarometerIdentity(identity))
    {
        Serial.println("Barometer identity: no readable ID/PROM at expected address 0x77");
        return;
    }

    Serial.printf("Barometer identity at 0x77: Bosch/DPS reg 0xD0=0x%02X, DPS reg 0x0D=0x%02X\n",
                  identity.boschChipId, identity.dps310ProductId);
    Serial.print("Barometer MS5611 PROM:");
    for (uint8_t index = 0; index < 7; ++index)
    {
        Serial.printf(" [%u]=0x%04X", index, identity.ms5611Prom[index]);
    }
    Serial.println();
}

BikeComputerData readBikeComputerData()
{
    gps::update();
    const gps::Data gpsData = gps::read();

    BikeComputerData data{};
    if (gpsData.hasTime)
    {
        data.time = gpsData.utcTime;
        data.timeValid = true;
        data.gpsTimeValid = true;

        if (!rtcSyncedFromGps)
        {
            rtcSyncedFromGps = sensors::setPcf8563(gpsData.utcTime);
            Serial.printf("GPS UTC time: %04u-%02u-%02u %02u:%02u:%02u | RTC update: %s\n",
                          gpsData.utcTime.year, gpsData.utcTime.month, gpsData.utcTime.day,
                          gpsData.utcTime.hour, gpsData.utcTime.minute, gpsData.utcTime.second,
                          rtcSyncedFromGps ? "complete" : "failed");
        }
    }
    else
    {
        data.timeValid = sensors::readPcf8563(data.time) && data.time.valid;
    }

    if (barometerReady)
    {
        const bool barometerRead =
            sensors::readMs5611(barometer, data.temperatureC, data.pressureHpa);
        const bool barometerInRange =
            data.temperatureC > -100.0F && data.temperatureC < 100.0F &&
            data.pressureHpa > 100.0F && data.pressureHpa < 1200.0F;

        if (barometerRead && barometerInRange)
        {
            data.altitudeM =
                44307.694F *
                (1.0F - std::pow(data.pressureHpa / SEA_LEVEL_PRESSURE_HPA, 0.190284F));
            data.barometerValid = true;
        }
        else if (barometerRead)
        {
            Serial.printf("MS5611 read out of expected range: temp=%.2f C pressure=%.2f hPa\n",
                          data.temperatureC, data.pressureHpa);
        }
        else
        {
            Serial.println("MS5611 read returned false");
        }
    }
    else
    {
        Serial.println("MS5611 skipped: init did not complete; retrying init");
        logI2cScan();
        logBarometerIdentity();
        tryInitBarometer();
    }

    if (!imuReady)
    {
        Serial.println("BMI270 skipped: init did not complete; retrying init");
        tryInitImu();
    }

    if (imuReady)
    {
        bmi2_sens_data sample{};
        if (bmi2_get_sensor_data(&sample, &imu) == BMI2_OK)
        {
            data.accelRawX = sample.acc.x;
            data.accelRawY = sample.acc.y;
            data.accelRawZ = sample.acc.z;
            const float x = sample.acc.x * accelScale;
            const float y = sample.acc.y * accelScale;
            const float z = sample.acc.z * accelScale;
            data.motionG = std::sqrt(x * x + y * y + z * z);
            data.inclineDegrees = std::atan2(x, std::sqrt(y * y + z * z)) * 180.0F / PI;
            data.imuValid = true;
        }
    }

    data.batteryV = getBatteryVoltage();
    data.gpsHasData = gpsData.hasData;
    data.gpsValid = gpsData.hasCoords;
    data.speedKph = gpsData.speedKph;
    data.latitude = gpsData.latitude;
    data.longitude = gpsData.longitude;
    data.gpsAltitudeM = gpsData.altitudeM;
    data.courseDeg = gpsData.courseDeg;
    data.satellitesUsed = gpsData.satellitesUsed;
    data.satellitesInView = gpsData.satellitesInView;
    return data;
}

void printData(const BikeComputerData &data)
{
    Serial.printf("Time: %02u:%02u:%02u %s | GPS: %s sats %d/%d | ",
                  data.time.hour, data.time.minute, data.time.second,
                  data.gpsTimeValid ? "GPS UTC" : "RTC",
                  data.gpsValid ? "fix" : "waiting",
                  data.satellitesUsed, data.satellitesInView);
    Serial.printf("Lat: %.6f | Lon: %.6f | Speed: %.1f km/h | Course: %.0f deg | ",
                  data.latitude, data.longitude, data.speedKph, data.courseDeg);
    Serial.printf("Altitude: %.1f m | Pressure: %.1f hPa | Temperature: %.1f C | ",
                  data.altitudeM, data.pressureHpa, data.temperatureC);
    Serial.printf("Accel raw: x=%d y=%d z=%d | Incline: %.1f deg | Motion: %.2f g | Battery: %.2f V\n",
                  data.accelRawX, data.accelRawY, data.accelRawZ,
                  data.inclineDegrees, data.motionG, data.batteryV);
}
} // namespace

void setup()
{
    Serial.begin(115200);
    Serial.println("Bike computer starting...");

    gps::begin();

    if (!sensors::initI2c())
    {
        Serial.println("I2C initialization failed");
        while (true)
        {
            gps::update();
            delay(1 * MILLISECONDS_PER_SECOND);
        }
    }
    Serial.println("I2C initialization complete");
    logI2cScan();
    logBarometerIdentity();

#if RTC_MANUAL_SYNC_ENABLED
    const sensors::DateTime manualRtcTime{
        RTC_SYNC_YEAR, RTC_SYNC_MONTH, RTC_SYNC_DAY, RTC_SYNC_WEEKDAY,
        RTC_SYNC_HOUR, RTC_SYNC_MINUTE, RTC_SYNC_SECOND, true};
    Serial.printf("Manual RTC sync: %s\n",
                  sensors::setPcf8563(manualRtcTime) ? "complete" : "failed");
#endif

    tryInitBarometer();
    tryInitImu();
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
            gps::update();
            delay(1 * MILLISECONDS_PER_SECOND);
        }
    }

    epd_poweron();
    epd_clear();
    epd_poweroff_all();
}

void loop()
{
    const BikeComputerData data = readBikeComputerData();
    printData(data);
    renderBikeComputer(framebuffer, data);

    const uint32_t refreshStartedAt = millis();
    while (millis() - refreshStartedAt < BIKE_COMPUTER_REFRESH_SECONDS * MILLISECONDS_PER_SECOND)
    {
        gps::update();
        delay(GPS_POLL_DELAY_MS);
    }
}
