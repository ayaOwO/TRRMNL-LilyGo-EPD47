#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, add board_build.extra_flags = -DBOARD_HAS_PSRAM"
#endif

/* *** My includes ********************************************* */
#include "wifi.hpp"
#include "cred.hpp"
#include <Button2.h>
#include <epd_driver.h>
// #include "Firasans/Firasans.h"
#include <Arduino.h>
#include "trmnl.hpp"

using namespace dashboard;

/* *** Hardware ********************************************* */
Button2 btn1(BUTTON_1);

/* *** Classes ********************************************** */
struct Cursor
{
  int x;
  int y;
};

/* *** Contsants ******************************************** */
const Rect_t text_area = {
    .x = 0, .y = 0, .width = EPD_WIDTH, .height = EPD_HEIGHT};

const int chars_in_line = 47;
const int rows_in_page = 10;

/* *** Globals ********************************************** */
uint8_t *framebuffer;
Cursor g_cursor = {.x = 20, .y = 60};
const int vref = 1100;
int is_sleep = 0;

/* *** Functions ******************************************** */

void enter_deep_sleep(void)
{
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
  bool success = fetch_and_convert_image(framebuffer, EPD_WIDTH * EPD_HEIGHT);
  Serial.printf("Image fetch and convert %s\n", success ? "succeeded" : "failed");
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
}

/* *** Setup ************************************************ */
uint8_t *get_new_frame_buffer(void)
{
  uint8_t *local_buffer = NULL;

  local_buffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT);
  // framebuffer = (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM);
  if (!local_buffer)
  {
    Serial.println("alloc memory failed !!!");
    while (1)
      ;
  }
  memset(local_buffer, 0xFF, EPD_WIDTH * EPD_HEIGHT);

  return local_buffer;
}

void setup()
{
  Serial.begin(115200);
  while (!Serial.availableForWrite());
  Serial.println("Dashboard starting...");
  epd_init();
  framebuffer = get_new_frame_buffer();

  btn1.setPressedHandler(buttonPressed);

  connectWifi();
  
}

void loop()
{
  btn1.loop();
  delay(100);
}
