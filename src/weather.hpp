/**
 * @brief Wifi utilities.
 * @author Aya Kraise
 */

#pragma once
#define MAX_DAYS 7

#include <Arduino.h>
#include <ArduinoJson.h>

namespace dashboard
{

    struct CurrentUnits
    {
        String time;
        String interval;
        String temperature_2m;
        String weather_code;
        String cloud_cover;
    };

    struct Current
    {
        String time;
        int interval;
        float temperature_2m;
        int weather_code;
        int cloud_cover;
    };

    struct DailyUnits
    {
        String time;
        String sunrise;
        String sunset;
        String precipitation_probability_max;
        String apparent_temperature_max;
        String apparent_temperature_min;
        String rain_sum;
    };

    struct Daily
    {
        String time[MAX_DAYS];
        String sunrise[MAX_DAYS];
        String sunset[MAX_DAYS];
        int precipitation_probability_max[MAX_DAYS];
        float apparent_temperature_max[MAX_DAYS];
        float apparent_temperature_min[MAX_DAYS];
        float rain_sum[MAX_DAYS];
        int count = 0; // how many entries actually present
    };

    typedef struct
    {
        float latitude;
        float longitude;
        float generationtime_ms;
        int utc_offset_seconds;
        String timezone;
        String timezone_abbreviation;
        float elevation;
        CurrentUnits current_units;
        Current current;
        DailyUnits daily_units;
        Daily daily;
    } WeatherResponse;
    WeatherResponse GetWeatherForecast(void);
    WeatherResponse JsonToWeatherResponse(JsonDocument doc);

}