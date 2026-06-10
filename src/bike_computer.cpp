#include "bike_computer.hpp"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

#include <epd_driver.h>

#include "Firasans/Firasans.h"

namespace
{
constexpr uint8_t BLACK = 0;
constexpr uint8_t DARK_GRAY = 5;

void text(uint8_t *framebuffer, int x, int y, const char *value)
{
    writeln(&FiraSans, value, &x, &y, framebuffer);
}

void box(uint8_t *framebuffer, int x, int y, int width, int height)
{
    epd_draw_rect(x, y, width, height, BLACK, framebuffer);
}

void metric(uint8_t *framebuffer, int x, int y, const char *label, const char *value)
{
    text(framebuffer, x, y, label);
    text(framebuffer, x, y + 53, value);
}
} // namespace

void renderBikeComputer(uint8_t *framebuffer, const BikeComputerData &data)
{
    std::memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    char value[40];
    text(framebuffer, 34, 54, "BIKE COMPUTER");
    if (data.timeValid)
    {
        std::snprintf(value, sizeof(value), "%02u:%02u", data.time.hour, data.time.minute);
    }
    else
    {
        std::snprintf(value, sizeof(value), "RTC --:--");
    }
    text(framebuffer, EPD_WIDTH - 190, 54, value);
    epd_draw_hline(28, 72, EPD_WIDTH - 56, DARK_GRAY, framebuffer);

    box(framebuffer, 28, 92, 470, 255);
    text(framebuffer, 56, 138, "SPEED");
    std::snprintf(value, sizeof(value), data.gpsValid ? "%.1f km/h" : "--.- km/h", data.speedKph);
    text(framebuffer, 56, 225, value);
    text(framebuffer, 56, 315, data.gpsValid ? "GPS READY" : "GPS WAITING - UART READY");

    box(framebuffer, 518, 92, EPD_WIDTH - 546, 255);
    text(framebuffer, 546, 138, "DISTANCE");
    std::snprintf(value, sizeof(value), data.gpsValid ? "%.2f km" : "--.-- km", data.distanceKm);
    text(framebuffer, 546, 225, value);
    text(framebuffer, 546, 315, "FUTURE GPS TRIP");

    box(framebuffer, 28, 367, EPD_WIDTH - 56, 145);
    std::snprintf(value, sizeof(value), data.barometerValid ? "%.0f m" : "-- m", data.altitudeM);
    metric(framebuffer, 55, 406, "ALTITUDE", value);
    std::snprintf(value, sizeof(value), data.barometerValid ? "%.1f C" : "-- C", data.temperatureC);
    metric(framebuffer, 250, 406, "TEMP", value);
    std::snprintf(value, sizeof(value), data.barometerValid ? "%.1f hPa" : "-- hPa", data.pressureHpa);
    metric(framebuffer, 410, 406, "PRESSURE", value);
    std::snprintf(value, sizeof(value), data.imuValid ? "%+.1f deg" : "-- deg", data.inclineDegrees);
    metric(framebuffer, 630, 406, "INCLINE", value);
    std::snprintf(value, sizeof(value), "%.2f V", data.batteryV);
    metric(framebuffer, 805, 406, "BATTERY", value);

    epd_poweron();
    epd_clear();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff_all();
}
