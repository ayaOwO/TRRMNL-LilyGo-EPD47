
#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, add board_build.extra_flags = -DBOARD_HAS_PSRAM"
#endif


/* *** My includes ********************************************* */
#include "wifi.hpp"
// #include "ntp.hpp"
#include "cred.hpp"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "Button2.h"
#include "epd_driver.h"
#include "firasans.h"
#include <Arduino.h>

using namespace dashboard;

/* *** Hardware ********************************************* */
Button2 btn1(BUTTON_1);

/* *** Classes ********************************************** */
struct Cursor {
  int x;
  int y;
};

class StringBuffer {
public:
  char *head;
  char *curr;
  size_t size;

  StringBuffer(size_t size) {
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
// const char *aliceInWonderLand = {

//     "CHAPTER I.\nDown the Rabbit-Hole\n\n\nAlice was beginning to get very tired of sitting by her sister on the\nbank, and of having nothing to do: once or twice she had peeped into\nthe book her sister was reading, but it had no pictures or\nconversations in it, “and what is the use of a book,” thought Alice\n“without pictures or conversations?”\n\nSo she was considering in her own mind (as well as she could, for the\nhot day made her feel very sleepy and stupid), whether the pleasure of\nmaking a daisy-chain would be worth the trouble of getting up and\npicking the daisies, when suddenly a White Rabbit with pink eyes ran\nclose by her."
// };

const int chars_in_line = 47;
const int rows_in_page = 10;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, 7200);

/* *** Globals ********************************************** */
uint8_t *framebuffer;
Cursor g_cursor = {.x = 20, .y = 60};
const int vref = 1100;
int is_sleep = 0;

/* *** Functions ******************************************** */
void reset_global_curser(void) {
  g_cursor.x = 20;
  g_cursor.y = 60;
}

void displayInfo(const char* text) {
  epd_poweron();
  epd_clear();
  reset_global_curser();
  write_string((GFXfont *)&FiraSans, text, &g_cursor.x,
               &g_cursor.y, NULL);
  epd_poweroff();
}

void enter_deep_sleep(void) {
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
void buttonPressed(Button2 &b) {
  Serial.println("Button was pressed");
  timeClient.update();
  Serial.println(timeClient.getFormattedTime().c_str());
  displayInfo(timeClient.getFormattedTime().c_str());
}

/* *** Setup ************************************************ */
uint8_t *get_new_frame_buffer(void) {
  uint8_t *local_buffer = NULL;

  local_buffer =
      (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!local_buffer) {
    Serial.println("alloc memory failed !!!");
    while (1)
      ;
  }
  memset(local_buffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  return local_buffer;
}

void setup() {
  Serial.begin(115200);
  while(!Serial.availableForWrite());
  epd_init();
  Serial.println("Hello world");
  // framebuffer = get_new_frame_buffer();

  btn1.setPressedHandler(buttonPressed);

  displayInfo("Connecting now");
  connectWifi();
  char s[50] = { 0 };
  sprintf(s, "Connected %s %s", WIFI_CREDS.WifiName.c_str(), WIFI_CREDS.Password.c_str());
  displayInfo(s);

  timeClient.begin();
  // epd_draw_grayscale_image(epd_full_screen(), framebuffer);
}

void loop() {
  timeClient.update();
  String time = timeClient.getFormattedTime();
  Serial.printf("Time is %s\n", time.c_str());
  displayInfo(time.c_str());
  btn1.loop();
  delay(2000);
}
