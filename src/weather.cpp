#include "weather.hpp"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

using namespace dashboard;

WeatherResponse dashboard::JsonToWeatherResponse(JsonDocument doc)
{
    WeatherResponse response{}; // Initialize empty struct
                                // Parse top-level fields
    response.latitude = doc["latitude"] | 0.0f;
    response.longitude = doc["longitude"] | 0.0f;
    response.generationtime_ms = doc["generationtime_ms"] | 0.0f;
    response.utc_offset_seconds = doc["utc_offset_seconds"] | 0;

    const char *tz = doc["timezone"] | nullptr;
    response.timezone = tz ? String(tz) : String("");

    const char *tzabbr = doc["timezone_abbreviation"] | nullptr;
    response.timezone_abbreviation = tzabbr ? String(tzabbr) : String("");

    response.elevation = doc["elevation"] | 0.0f;

    // Parse current_units
    JsonObject current_units = doc["current_units"];
    const char *cu_time = current_units["time"] | nullptr;
    response.current_units.time = cu_time ? String(cu_time) : String("");

    const char *cu_interval = current_units["interval"] | nullptr;
    response.current_units.interval = cu_interval ? String(cu_interval) : String("");

    const char *cu_temp = current_units["temperature_2m"] | nullptr;
    response.current_units.temperature_2m = cu_temp ? String(cu_temp) : String("");

    const char *cu_wcode = current_units["weather_code"] | nullptr;
    response.current_units.weather_code = cu_wcode ? String(cu_wcode) : String("");

    const char *cu_cloud = current_units["cloud_cover"] | nullptr;
    response.current_units.cloud_cover = cu_cloud ? String(cu_cloud) : String("");

    // Parse current
    JsonObject current = doc["current"];
    const char *c_time = current["time"] | nullptr;
    response.current.time = c_time ? String(c_time) : String("");
    response.current.interval = current["interval"] | 0;
    response.current.temperature_2m = current["temperature_2m"] | 0.0f;
    response.current.weather_code = current["weather_code"] | 0;
    response.current.cloud_cover = current["cloud_cover"] | 0;

    // Parse daily_units
    JsonObject daily_units = doc["daily_units"];
    const char *du_time = daily_units["time"] | nullptr;
    response.daily_units.time = du_time ? String(du_time) : String("");

    const char *du_sunrise = daily_units["sunrise"] | nullptr;
    response.daily_units.sunrise = du_sunrise ? String(du_sunrise) : String("");

    const char *du_sunset = daily_units["sunset"] | nullptr;
    response.daily_units.sunset = du_sunset ? String(du_sunset) : String("");

    const char *du_precip = daily_units["precipitation_probability_max"] | nullptr;
    response.daily_units.precipitation_probability_max = du_precip ? String(du_precip) : String("");

    const char *du_app_max = daily_units["apparent_temperature_max"] | nullptr;
    response.daily_units.apparent_temperature_max = du_app_max ? String(du_app_max) : String("");

    const char *du_app_min = daily_units["apparent_temperature_min"] | nullptr;
    response.daily_units.apparent_temperature_min = du_app_min ? String(du_app_min) : String("");

    const char *du_rain = daily_units["rain_sum"] | nullptr;
    response.daily_units.rain_sum = du_rain ? String(du_rain) : String("");

    // Parse daily arrays
    JsonObject daily = doc["daily"];
    JsonArray times = daily["time"];
    JsonArray sunrises = daily["sunrise"];
    JsonArray sunsets = daily["sunset"];
    JsonArray precipitation_probability_max = daily["precipitation_probability_max"];
    JsonArray apparent_temperature_max = daily["apparent_temperature_max"];
    JsonArray apparent_temperature_min = daily["apparent_temperature_min"];
    JsonArray rain_sum = daily["rain_sum"];

    int count = times.size();
    if (count > MAX_DAYS)
        count = MAX_DAYS;
    response.daily.count = count;

    for (int i = 0; i < count; i++)
    {
        const char *timeStr = times[i] | nullptr;
        response.daily.time[i] = timeStr ? String(timeStr) : String("");

        const char *sunriseStr = sunrises[i] | nullptr;
        response.daily.sunrise[i] = sunriseStr ? String(sunriseStr) : String("");

        const char *sunsetStr = sunsets[i] | nullptr;
        response.daily.sunset[i] = sunsetStr ? String(sunsetStr) : String("");

        response.daily.precipitation_probability_max[i] = precipitation_probability_max[i] | 0;
        response.daily.apparent_temperature_max[i] = apparent_temperature_max[i] | 0.0f;
        response.daily.apparent_temperature_min[i] = apparent_temperature_min[i] | 0.0f;
        response.daily.rain_sum[i] = rain_sum[i] | 0.0f;
    }
    return response;
}
WeatherResponse dashboard::GetWeatherForecast(void)
{
    WeatherResponse response{}; // Initialize empty struct

    if (WiFi.status() == WL_CONNECTED)
    {
        String url = "https://api.open-meteo.com/v1/forecast?latitude=31.9987&longitude=34.9456&daily=sunrise,sunset,precipitation_probability_max,apparent_temperature_max,apparent_temperature_min,rain_sum&current=temperature_2m,weather_code,cloud_cover&timezone=auto&forecast_days=1&wind_speed_unit=ms";
        HTTPClient http;
        http.begin(url);           // Specify the URL
        int httpCode = http.GET(); // Make the GET request

        if (httpCode > 0)
        {
            String payload = http.getString();
            Serial.println("Response:");
            Serial.println(payload);

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            if (error)
            {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.f_str());
                return response;
            }
            response = dashboard::JsonToWeatherResponse(doc);
        }
        else
        {
            Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
        }

        http.end(); // Free resources
    }
    else
    {
        Serial.println("WiFi not connected");
    }

    return response;
}