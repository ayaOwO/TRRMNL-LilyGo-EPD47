#include "bike_computer.hpp"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

#include <epd_driver.h>

#include "Firasans/Firasans.h"

namespace
{
#define BIKE_UI_BLACK 0
#define BIKE_UI_DARK_GRAY 5

#define BIKE_UI_TITLE_X 34
#define BIKE_UI_TITLE_Y 54
#define BIKE_UI_TIME_X (EPD_WIDTH - 210)
#define BIKE_UI_TIME_Y 54
#define BIKE_UI_RULE_X 28
#define BIKE_UI_RULE_Y 72
#define BIKE_UI_RULE_W (EPD_WIDTH - 56)

#define BIKE_UI_SPEED_BOX_X 28
#define BIKE_UI_SPEED_BOX_Y 92
#define BIKE_UI_SPEED_BOX_W 470
#define BIKE_UI_SPEED_BOX_H 255
#define BIKE_UI_SPEED_LABEL_X 56
#define BIKE_UI_SPEED_LABEL_Y 138
#define BIKE_UI_SPEED_VALUE_X 56
#define BIKE_UI_SPEED_VALUE_Y 225
#define BIKE_UI_SPEED_STATUS_X 56
#define BIKE_UI_SPEED_STATUS_Y 315

#define BIKE_UI_GPS_BOX_X 518
#define BIKE_UI_GPS_BOX_Y 92
#define BIKE_UI_GPS_BOX_W (EPD_WIDTH - 546)
#define BIKE_UI_GPS_BOX_H 255
#define BIKE_UI_GPS_LABEL_X 546
#define BIKE_UI_GPS_LABEL_Y 138
#define BIKE_UI_GPS_COORDS_X 546
#define BIKE_UI_GPS_COORDS_Y 205
#define BIKE_UI_GPS_COURSE_X 546
#define BIKE_UI_GPS_COURSE_Y 270
#define BIKE_UI_GPS_ALT_X 546
#define BIKE_UI_GPS_ALT_Y 325
#define BIKE_UI_GPS_WAITING_X 546
#define BIKE_UI_GPS_WAITING_Y 225
#define BIKE_UI_GPS_WAITING_SATS_X 546
#define BIKE_UI_GPS_WAITING_SATS_Y 315

#define BIKE_UI_METRIC_BOX_X 28
#define BIKE_UI_METRIC_BOX_Y 367
#define BIKE_UI_METRIC_BOX_W (EPD_WIDTH - 56)
#define BIKE_UI_METRIC_BOX_H 145
#define BIKE_UI_METRIC_LABEL_Y 406
#define BIKE_UI_METRIC_VALUE_Y 459
#define BIKE_UI_ALTITUDE_X 55
#define BIKE_UI_TEMP_X 250
#define BIKE_UI_PRESSURE_X 410
#define BIKE_UI_INCLINE_X 630
#define BIKE_UI_BATTERY_X 805

void text(uint8_t *framebuffer, int x, int y, const char *value)
{
    writeln(&FiraSans, value, &x, &y, framebuffer);
}

void box(uint8_t *framebuffer, int x, int y, int width, int height)
{
    epd_draw_rect(x, y, width, height, BIKE_UI_BLACK, framebuffer);
}

void metric(uint8_t *framebuffer, int x, const char *label, const char *value)
{
    text(framebuffer, x, BIKE_UI_METRIC_LABEL_Y, label);
    text(framebuffer, x, BIKE_UI_METRIC_VALUE_Y, value);
}
} // namespace

void renderBikeComputer(uint8_t *framebuffer, const BikeComputerData &data)
{
    std::memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    char value[64];
    text(framebuffer, BIKE_UI_TITLE_X, BIKE_UI_TITLE_Y, "BIKE COMPUTER");
    if (data.timeValid)
    {
        std::snprintf(value, sizeof(value), "%02u:%02u", data.time.hour, data.time.minute);
    }
    else
    {
        std::snprintf(value, sizeof(value), "RTC --:--");
    }
    text(framebuffer, BIKE_UI_TIME_X, BIKE_UI_TIME_Y, value);
    epd_draw_hline(BIKE_UI_RULE_X, BIKE_UI_RULE_Y, BIKE_UI_RULE_W, BIKE_UI_DARK_GRAY, framebuffer);

    box(framebuffer, BIKE_UI_SPEED_BOX_X, BIKE_UI_SPEED_BOX_Y, BIKE_UI_SPEED_BOX_W, BIKE_UI_SPEED_BOX_H);
    text(framebuffer, BIKE_UI_SPEED_LABEL_X, BIKE_UI_SPEED_LABEL_Y, "SPEED");
    std::snprintf(value, sizeof(value), data.gpsValid ? "%.1f km/h" : "--.- km/h", data.speedKph);
    text(framebuffer, BIKE_UI_SPEED_VALUE_X, BIKE_UI_SPEED_VALUE_Y, value);
    text(framebuffer, BIKE_UI_SPEED_STATUS_X, BIKE_UI_SPEED_STATUS_Y, data.gpsValid ? "GPS FIX" : "GPS WAITING");

    box(framebuffer, BIKE_UI_GPS_BOX_X, BIKE_UI_GPS_BOX_Y, BIKE_UI_GPS_BOX_W, BIKE_UI_GPS_BOX_H);
    text(framebuffer, BIKE_UI_GPS_LABEL_X, BIKE_UI_GPS_LABEL_Y, "GPS");
    if (data.gpsValid)
    {
        std::snprintf(value, sizeof(value), "%.6f, %.6f", data.latitude, data.longitude);
        text(framebuffer, BIKE_UI_GPS_COORDS_X, BIKE_UI_GPS_COORDS_Y, value);
        std::snprintf(value, sizeof(value), "COURSE %.0f deg  SAT %d/%d",
                      data.courseDeg, data.satellitesUsed, data.satellitesInView);
        text(framebuffer, BIKE_UI_GPS_COURSE_X, BIKE_UI_GPS_COURSE_Y, value);
        std::snprintf(value, sizeof(value), "GPS ALT %.0f m", data.gpsAltitudeM);
        text(framebuffer, BIKE_UI_GPS_ALT_X, BIKE_UI_GPS_ALT_Y, value);
    }
    else
    {
        text(framebuffer,
             BIKE_UI_GPS_WAITING_X,
             BIKE_UI_GPS_WAITING_Y,
             data.gpsHasData ? "NO GPS FIX" : "NO GPS MODULE");
        std::snprintf(value, sizeof(value), "SAT %d/%d", data.satellitesUsed, data.satellitesInView);
        text(framebuffer, BIKE_UI_GPS_WAITING_SATS_X, BIKE_UI_GPS_WAITING_SATS_Y, value);
    }

    box(framebuffer, BIKE_UI_METRIC_BOX_X, BIKE_UI_METRIC_BOX_Y, BIKE_UI_METRIC_BOX_W, BIKE_UI_METRIC_BOX_H);
    std::snprintf(value, sizeof(value), data.barometerValid ? "%.0f m" : "-- m", data.altitudeM);
    metric(framebuffer, BIKE_UI_ALTITUDE_X, "ALTITUDE", value);
    std::snprintf(value, sizeof(value), data.barometerValid ? "%.1f C" : "-- C", data.temperatureC);
    metric(framebuffer, BIKE_UI_TEMP_X, "TEMP", value);
    std::snprintf(value, sizeof(value), data.barometerValid ? "%.1f hPa" : "-- hPa", data.pressureHpa);
    metric(framebuffer, BIKE_UI_PRESSURE_X, "PRESSURE", value);
    std::snprintf(value, sizeof(value), data.imuValid ? "%+.1f deg" : "-- deg", data.inclineDegrees);
    metric(framebuffer, BIKE_UI_INCLINE_X, "INCLINE", value);
    std::snprintf(value, sizeof(value), "%.2f V", data.batteryV);
    metric(framebuffer, BIKE_UI_BATTERY_X, "BATTERY", value);

    epd_poweron();
    epd_clear();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff_all();
}
