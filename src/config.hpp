#define BATTERY_CALIBRATIONFACTOR 1
#define BIKE_COMPUTER_REFRESH_SECONDS 10
#define NTP_SYNC_TIMEOUT_SECONDS 20
#define NTP_SERVER "pool.ntp.org"
#define NTP_TIMEZONE "IST-2IDT,M3.4.4/26,M10.5.0"

// Set to 1 for one flash only, then set back to 0 and flash again.
#define RTC_MANUAL_SYNC_ENABLED 0
#define RTC_SYNC_YEAR 2026
#define RTC_SYNC_MONTH 6
#define RTC_SYNC_DAY 8
#define RTC_SYNC_WEEKDAY 1 // Sunday=0, Monday=1
#define RTC_SYNC_HOUR 16
#define RTC_SYNC_MINUTE 31
#define RTC_SYNC_SECOND 54

#include "secrets.hpp"
