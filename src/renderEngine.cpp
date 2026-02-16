#include "renderEngine.hpp"
#include <Arduino.h>
#include <epd_driver.h>
#include "trmnl.hpp"
#include "pic3.h" // - Contains the image1 C-array

int render_frame(uint8_t *local_framebuffer)
{
  DisplayConfig config = get_display_config(); //
  
  if (!config.success) {
    Serial.println("Failed to get display config");
    return 60;
  }

  bool success = false;

  // Check if the URL indicates the sleep screen placeholder
  if (config.image_url == "null" || config.image_url.endsWith("sleep.bmp")) {
    epd_copy_to_framebuffer(epd_full_screen(), (uint8_t *)pic3_data, local_framebuffer);
    Serial.println("Sleep URL detected. Loading placeholder image1");
    success = true;
    if (config.refresh_rate < 60)
      config.refresh_rate = 60;
  } 
  else {
    // Standard path: Fetch and convert from the network
    success = fetch_and_convert_image(config.image_url.c_str(), local_framebuffer, EPD_WIDTH * EPD_HEIGHT / 2);
    Serial.printf("Image fetch and convert %s\n", success ? "succeeded" : "failed");
  }
  
  if (success) {
      epd_poweron();
      epd_clear();
      epd_draw_grayscale_image(epd_full_screen(), local_framebuffer);
      epd_poweroff_all();
  }

  return config.refresh_rate;
}

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