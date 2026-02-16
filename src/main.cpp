#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, add board_build.extra_flags = -DBOARD_HAS_PSRAM"
#endif

/* *** My includes ********************************************* */
#include "wifi.hpp"
#include <Button2.h>
#include <epd_driver.h>
#include "battery.hpp"
#include "renderEngine.hpp"
#include <Arduino.h>

using namespace dashboard;

/* *** Hardware ********************************************* */
Button2 btn1(BUTTON_1);

/* *** Contsants ******************************************** */

/* *** Globals ********************************************** */
uint8_t *framebuffer = NULL;

/* *** Functions ******************************************** */



/* *** Events *********************************************** */
void buttonPressed(Button2 &b)
{
  Serial.println("Button was pressed");
  float voltage = getBatteryVoltage();
  Serial.printf("Battery Voltage: %.2f V\n", voltage);
  render_frame(framebuffer);
}

/* *** Setup ************************************************ */


void setup()
{
  // Serial.begin(115200);
  // while (!Serial.availableForWrite());
  setCpuFrequencyMhz(80);
  Serial.println("Dashboard starting...");
  epd_init();
  framebuffer = init_framebuffer();

  btn1.setPressedHandler(buttonPressed);

  connectWifi();
  
  int timeout = render_frame(framebuffer);
  sleep_for_seconds(timeout);
}

void loop()
{
  // btn1.loop();
  // delay(100);
}
