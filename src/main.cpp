#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, add board_build.extra_flags = -DBOARD_HAS_PSRAM"
#endif
// #define DISABLE_PLAYER
// #define DISABLE_ALBUM
// #define DISABLE_ARTIST
// #define DISABLE_AUDIOBOOKS
// #define DISABLE_CATEGORIES
// #define DISABLE_CHAPTERS
// #define DISABLE_EPISODES
// #define DISABLE_GENRES
// #define DISABLE_MARKETS
// #define DISABLE_PLAYLISTS
// #define DISABLE_SEARCH
// #define DISABLE_SHOWS
// #define DISABLE_TRACKS
// #define DISABLE_USER
// #define DISABLE_SIMPLIFIED
#define DISABLE_WEB_SERVER

/* *** My includes ********************************************* */
#include "weather.hpp"
#include "wifi.hpp"
#include "cred.hpp"
#include <Button2.h>
#include <epd_driver.h>
#include "Firasans/Firasans.h"
#include <SpotifyEsp32.h>
#include <Arduino.h>

using namespace dashboard;

/* *** Hardware ********************************************* */
Button2 btn1(BUTTON_1);

/* *** Classes ********************************************** */
struct Cursor
{
  int x;
  int y;
};

class StringBuffer
{
public:
  char *head;
  char *curr;
  size_t size;

  StringBuffer(size_t size)
  {
    this->size = size;
    this->head = (char *)ps_calloc(sizeof(*this->head), size);
    this->curr = this->head;
  }

  const inline size_t curr_off(void) { return this->curr - this->head; }

  inline void insert_char(char c) { *(this->curr++) = c; }
};

/* *** Contsants ******************************************** */
const Rect_t text_area = {
    .x = 10, .y = 20, .width = EPD_WIDTH - 20, .height = EPD_HEIGHT - 10};

const int chars_in_line = 47;
const int rows_in_page = 10;

/* *** Globals ********************************************** */
uint8_t *framebuffer;
Cursor g_cursor = {.x = 20, .y = 60};
const int vref = 1100;
int is_sleep = 0;
Spotify *spotify = nullptr;

/* *** Functions ******************************************** */
void reset_global_curser(void)
{
  g_cursor.x = 20;
  g_cursor.y = 60;
}

void displayInfo(consst char *text)
{
  epd_poweron();
  epd_clear();
  reset_global_curser();
  write_string((GFXfont *)&FiraSans, text, &g_cursor.x,
               &g_cursor.y, NULL);
  epd_poweroff();
}

void enter_deep_sleep(void)
{
  delay(1000);
  epd_clear_area(text_area);
  reset_global_curser();
  write_string((GFXfont *)&FiraSans, "Deep Sleep", &g_cursor.x, &g_cursor.y,
               NULL);
  epd_poweroff_all();
#if defined(CONFIG_IDF_TARGET_ESP32)
  // Set to wake up by GPIO39
  esp_sleep_enable_ext1_wakeup(GPIO_SEL_39, ESP_EXT1_WAKEUP_ANY_LOW);
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  esp_sleep_enable_ext1_wakeup(GPIO_SEL_21, ESP_EXT1_WAKEUP_ANY_LOW);
#endif
  esp_deep_sleep_start();
}

/* *** Events *********************************************** */
void buttonPressed(Button2 &b)
{
  Serial.println("Button was pressed");
  String currentTrack = spotify->current_track_name();
  Serial.printf("Currently playing: %s\n", currentTrack.c_str());

  displayInfo(currentTrack.c_str());
}

/* *** Setup ************************************************ */
void logTokens(Spotify *sp)
{
  user_tokens tokens = sp->get_user_tokens();
  Serial.println("---- Spotify Tokens ----");
  Serial.printf("Client ID: %s\n", tokens.client_id);
  Serial.printf("Client Secret: %s\n", tokens.client_secret);
  Serial.printf("Refresh Token: %s\n", tokens.refresh_token);

  if (sp->has_access_token())
  {
    Serial.println("Access token is currently valid.");
  }
  else
  {
    Serial.println("No valid access token available.");
  }
}
uint8_t *get_new_frame_buffer(void)
{
  uint8_t *local_buffer = NULL;

  local_buffer =
      (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!local_buffer)
  {
    Serial.println("alloc memory failed !!!");
    while (1)
      ;
  }
  memset(local_buffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  return local_buffer;
}
void setTimezone()
{

  // Israel TZ with DST rules
  configTzTime("Asia/Jerusalem", "3.il.pool.ntp.org", "1.asia.pool.ntp.org", "3.asia.pool.ntp.org"); // sync time first

  struct tm timeinfo;
  while (!getLocalTime(&timeinfo))
  {
    Serial.println("Waiting for NTP sync...");
    delay(1000);
  }

  Serial.println("Time synchronized");
}

std::string GetFormattedTime()
{
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%H:%M %d.%m.%Y", &timeinfo);
    return std::string(buffer);
  }
  return std::string("00:00 00.00.0000");
}

void setup()
{
  Serial.begin(115200);
  while (!Serial.availableForWrite())
    ;
  epd_init();
  // framebuffer = get_new_frame_buffer();

  btn1.setPressedHandler(buttonPressed);

  displayInfo("Connecting...");
  connectWifi();
  char s[50] = {0};
  sprintf(s, "Connected as %s %s %s", WiFi.localIP().toString().c_str(), WIFI_CREDS.WifiName.c_str(), WIFI_CREDS.Password.c_str());
  setTimezone();

  // spotify = new Spotify(SPOTIFY_CREDS.ClientId.c_str(), SPOTIFY_CREDS.ClientSecret.c_str(), SPOTIFY_CREDS.RefreshToken.c_str(), 80, true, 3);

  // epd_draw_grayscale_image(epd_full_screen(), framebuffer);
}

void loop()
{
  char s[100] = {0};
  btn1.loop();
  WeatherResponse weather = GetWeatherForecast();
  sprintf(s, "%s\nMax %.2f%s %.2f%s %d%s",
          GetFormattedTime().c_str(),
          weather.daily.apparent_temperature_max[0],
          weather.daily_units.apparent_temperature_max.c_str(),
          weather.daily.apparent_temperature_min[0],
          weather.daily_units.apparent_temperature_min.c_str(),
          weather.daily.precipitation_probability_max[0],
          weather.daily_units.precipitation_probability_max.c_str());
  displayInfo(s);
  Serial.println(strlen(s));
  delay(5000);
}
