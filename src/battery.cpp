#include "battery.hpp"
#include <Arduino.h>
#include <epd_driver.h>
#include "config.hpp"

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */

float getBatteryVoltage() {
  Serial.println("--- Battery Check ---");
  
  // 1. Configure the ADC
  analogSetAttenuation(ADC_11db); 
  
  // 2. Sample and Average
  uint32_t rawSum = 0;
  int samples = 10;
  
  for (int i = 0; i < samples; i++) {
    uint16_t reading = analogRead(BATT_PIN);
    rawSum += reading;
    Serial.printf("Sample %d: %u\n", i, reading); // Uncomment for deep debugging
    delay(2); 
  }
  
  float rawAverage = (float)rawSum / samples;
  
  // 3. Convert to Voltage
  // Formula: (ADC / 4095) * 3.3V * 2 (for the 100k/100k divider)
  float voltage = (rawAverage / 4095.0) * 3.3 * 2.0;

  // 4. Calibration
  float finalVoltage = voltage * CALIBRATIONFACTOR;

  // --- Serial Logging ---
  Serial.printf("Raw ADC Avg: %.2f\n", rawAverage);
  Serial.printf("Calculated:  %.2f V\n", voltage);
  Serial.printf("Calibrated:  %.2f V\n", finalVoltage);
  
  if (finalVoltage < 3.3) {
    Serial.println("Warning: Battery Low!");
  } else if (finalVoltage > 4.25) {
    Serial.println("Status: Charging / USB Power");
  } else {
    Serial.println("Status: Battery Healthy");
  }
  Serial.println("---------------------");

  return finalVoltage;
}

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
  esp_sleep_enable_ext1_wakeup(GPIO_SEL_21, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}