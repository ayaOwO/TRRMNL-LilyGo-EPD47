#include "wifi_time.hpp"

#include <Arduino.h>
#include <WiFi.h>
#include <ctime>

#include "config.hpp"
#include "sensors.hpp"

namespace
{
void stopWifi()
{
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}
} // namespace

bool syncRtcFromNtp()
{
    Serial.printf("Connecting to Wi-Fi \"%s\" for NTP", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    const unsigned long wifiDeadline = millis() + NTP_SYNC_TIMEOUT_SECONDS * 1000UL;
    while (WiFi.status() != WL_CONNECTED && millis() < wifiDeadline)
    {
        delay(250);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Wi-Fi connection timed out; keeping existing RTC time");
        stopWifi();
        return false;
    }

    configTzTime(NTP_TIMEZONE, NTP_SERVER);
    tm localTime{};
    const bool timeReady = getLocalTime(&localTime, NTP_SYNC_TIMEOUT_SECONDS * 1000UL);
    if (!timeReady)
    {
        Serial.println("NTP sync timed out; keeping existing RTC time");
        stopWifi();
        return false;
    }

    const sensors::DateTime rtcTime{
        static_cast<uint16_t>(localTime.tm_year + 1900),
        static_cast<uint8_t>(localTime.tm_mon + 1),
        static_cast<uint8_t>(localTime.tm_mday),
        static_cast<uint8_t>(localTime.tm_wday),
        static_cast<uint8_t>(localTime.tm_hour),
        static_cast<uint8_t>(localTime.tm_min),
        static_cast<uint8_t>(localTime.tm_sec),
        true,
    };
    const bool rtcUpdated = sensors::setPcf8563(rtcTime);
    Serial.printf("NTP time: %04u-%02u-%02u %02u:%02u:%02u | RTC update: %s\n",
                  rtcTime.year, rtcTime.month, rtcTime.day,
                  rtcTime.hour, rtcTime.minute, rtcTime.second,
                  rtcUpdated ? "complete" : "failed");
    stopWifi();
    return rtcUpdated;
}
