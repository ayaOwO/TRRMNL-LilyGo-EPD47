#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, add board_build.extra_flags = -DBOARD_HAS_PSRAM"
#endif

/* *** My includes ********************************************* */
#include "wifi.hpp"
#include "cred.hpp"
#include <Button2.h>
#include <epd_driver.h>
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10          /* Time ESP32 will go to sleep (in seconds) */
// #include "Firasans/Firasans.h"
#include <Arduino.h>
#include "trmnl.hpp"

using namespace dashboard;

/* *** Hardware ********************************************* */
Button2 btn1(BUTTON_1);

/* *** Contsants ******************************************** */

/* *** Globals ********************************************** */
uint8_t *framebuffer;

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

void sleep_for_seconds(int seconds)
{
  Serial.printf("Sleeping for %d seconds...\n", seconds);
  Serial.flush();
  esp_sleep_enable_timer_wakeup(seconds * uS_TO_S_FACTOR);
  epd_poweroff_all();
  esp_deep_sleep_start();
}

/* *** Events *********************************************** */
void buttonPressed(Button2 &b)
{
  Serial.println("Button was pressed");
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

int render_frame()
{
  uint8_t * temp = init_framebuffer();
  DisplayConfig config = get_display_config();
  if (!config.success) {
    Serial.println("Failed to get display config");
    return 60;
  }
  bool success = fetch_and_convert_image(config.image_url.c_str(), temp, EPD_WIDTH * EPD_HEIGHT / 2);
  Serial.printf("Image fetch and convert %s\n", success ? "succeeded" : "failed");
  epd_copy_to_framebuffer(epd_full_screen(), temp, framebuffer);

  epd_poweron();
  epd_clear();
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();
  heap_caps_free(temp);
  return config.refresh_rate;
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
  
  int timeout = render_frame();
  sleep_for_seconds(timeout);
}

void loop()
{
  // btn1.loop();
  // delay(100);
}
