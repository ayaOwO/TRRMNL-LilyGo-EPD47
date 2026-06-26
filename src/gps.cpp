#include "gps.hpp"

#include <Arduino.h>

namespace
{
HardwareSerial GPS(1);

constexpr int GPS_RX_PIN = 48;
constexpr int GPS_TX_PIN = -1;
constexpr uint32_t GPS_BAUD = 115200;

bool started = false;
String lineBuffer;
gps::Data state{};

float parseFloatField(const String &value)
{
    return value.length() > 0 ? value.toFloat() : 0.0F;
}

int parseIntField(const String &value)
{
    return value.length() > 0 ? value.toInt() : 0;
}

String fieldAt(const String &line, int index)
{
    int fieldStart = 0;
    int currentIndex = 0;

    for (int i = 0; i <= line.length(); i++)
    {
        if (i == line.length() || line[i] == ',' || line[i] == '*')
        {
            if (currentIndex == index)
            {
                return line.substring(fieldStart, i);
            }

            fieldStart = i + 1;
            currentIndex++;
        }
    }

    return "";
}

String nmeaType(const String &line)
{
    if (line.length() < 6 || line[0] != '$')
    {
        return "";
    }

    return line.substring(3, 6);
}

float parseCoordinate(const String &value, const String &hemisphere)
{
    const int dotIndex = value.indexOf('.');
    if (dotIndex < 0)
    {
        return 0.0F;
    }

    const int degreeDigits = dotIndex > 4 ? 3 : 2;
    const float degrees = value.substring(0, degreeDigits).toFloat();
    const float minutes = value.substring(degreeDigits).toFloat();
    float coordinate = degrees + minutes / 60.0F;
    if (hemisphere == "S" || hemisphere == "W")
    {
        coordinate = -coordinate;
    }
    return coordinate;
}

uint8_t weekdayForDate(uint16_t year, uint8_t month, uint8_t day)
{
    if (month < 3)
    {
        month += 12;
        year--;
    }

    const uint16_t k = year % 100;
    const uint16_t j = year / 100;
    const uint8_t h = (day + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    return (h + 6) % 7;
}

bool parseUtcDateTime(const String &timeField, const String &dateField, sensors::DateTime &dateTime)
{
    if (timeField.length() < 6 || dateField.length() != 6)
    {
        return false;
    }

    const uint8_t hour = timeField.substring(0, 2).toInt();
    const uint8_t minute = timeField.substring(2, 4).toInt();
    const uint8_t second = timeField.substring(4, 6).toInt();
    const uint8_t day = dateField.substring(0, 2).toInt();
    const uint8_t month = dateField.substring(2, 4).toInt();
    const uint8_t year2 = dateField.substring(4, 6).toInt();
    const uint16_t year = static_cast<uint16_t>(year2 >= 80 ? 1900 + year2 : 2000 + year2);

    if (hour > 23 || minute > 59 || second > 59 || day < 1 || day > 31 || month < 1 || month > 12)
    {
        return false;
    }

    dateTime = {
        year,
        month,
        day,
        weekdayForDate(year, month, day),
        hour,
        minute,
        second,
        true,
    };
    return true;
}

void parseGga(const String &line)
{
    state.latitude = parseCoordinate(fieldAt(line, 2), fieldAt(line, 3));
    state.longitude = parseCoordinate(fieldAt(line, 4), fieldAt(line, 5));
    state.fixQuality = parseIntField(fieldAt(line, 6));
    state.satellitesUsed = parseIntField(fieldAt(line, 7));
    state.hdop = parseFloatField(fieldAt(line, 8));
    state.altitudeM = parseFloatField(fieldAt(line, 9));
    state.hasFix = state.fixQuality > 0;
    state.hasCoords = state.hasFix;
}

void parseRmc(const String &line)
{
    const bool active = fieldAt(line, 2) == "A";
    state.hasFix = active;
    state.latitude = parseCoordinate(fieldAt(line, 3), fieldAt(line, 4));
    state.longitude = parseCoordinate(fieldAt(line, 5), fieldAt(line, 6));
    state.speedKph = parseFloatField(fieldAt(line, 7)) * 1.852F;
    state.courseDeg = parseFloatField(fieldAt(line, 8));
    state.hasCoords = active;
    state.hasTime = active && parseUtcDateTime(fieldAt(line, 1), fieldAt(line, 9), state.utcTime);
}

void parseGsa(const String &line)
{
    state.fixMode = parseIntField(fieldAt(line, 2));
}

void parseGsv(const String &line)
{
    state.satellitesInView = parseIntField(fieldAt(line, 3));
}

void parseNmea(const String &line)
{
    const String type = nmeaType(line);
    if (type == "GGA")
    {
        parseGga(line);
    }
    else if (type == "RMC")
    {
        parseRmc(line);
    }
    else if (type == "GSA")
    {
        parseGsa(line);
    }
    else if (type == "GSV")
    {
        parseGsv(line);
    }
}

void finishLine()
{
    lineBuffer.trim();
    if (lineBuffer.startsWith("$GP") || lineBuffer.startsWith("$GN"))
    {
        parseNmea(lineBuffer);
    }
    lineBuffer = "";
}
} // namespace

namespace gps
{
void begin()
{
    if (started)
    {
        return;
    }

    GPS.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    started = true;
    Serial.printf("GPS started: UART1 RX GPIO%d, TX disabled, %lu baud\n",
                  GPS_RX_PIN, static_cast<unsigned long>(GPS_BAUD));
}

void update()
{
    if (!started)
    {
        begin();
    }

    while (GPS.available() > 0)
    {
        const char c = static_cast<char>(GPS.read());
        if (c == '\n')
        {
            finishLine();
        }
        else if (c != '\r')
        {
            lineBuffer += c;
        }
    }
}

Data read()
{
    return state;
}
}
