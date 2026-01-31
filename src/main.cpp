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
#include "pic2.h"
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
    .x = 0, .y = 0, .width = EPD_WIDTH, .height = EPD_HEIGHT / 2};

const int chars_in_line = 47;
const int rows_in_page = 10;

/* *** Globals ********************************************** */
uint8_t *framebuffer;
Cursor g_cursor = {.x = 20, .y = 60};
const int vref = 1100;
int is_sleep = 0;

/* *** Functions ******************************************** */
uint8_t *init_framebuffer(void);

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
  epd_poweron();
  epd_clear();
  Serial.println("Button was pressed");
  uint8_t * temp = init_framebuffer();
  bool success = fetch_and_convert_image(temp, EPD_WIDTH * EPD_HEIGHT / 2);
  Serial.printf("Image fetch and convert %s\n", success ? "succeeded" : "failed");
  epd_copy_to_framebuffer(epd_full_screen(), temp, framebuffer);
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();
  heap_caps_free(temp);
}

/* *** Setup ************************************************ */
uint8_t *init_framebuffer(void)
{
  uint8_t * local_framebuffer= NULL;

  local_framebuffer = (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM);
  if (!local_framebuffer)
  {
    Serial.println("alloc memory failed !!!");
    while (1)
      ;
  }
  memset(local_framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT/2);

  return local_framebuffer;
}

void setup()
{
  Serial.begin(115200);
  // while (!Serial.availableForWrite());
  Serial.println("Dashboard starting...");
  epd_init();
  framebuffer = init_framebuffer();

  btn1.setPressedHandler(buttonPressed);

  connectWifi();
  
}

void loop()
{
  btn1.loop();
  Serial.println("Awaiting button press...");
  delay(100);
}
