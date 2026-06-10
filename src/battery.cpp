#include "battery.hpp"
#include <Arduino.h>
#include <epd_driver.h>
#include "config.hpp"

float getBatteryVoltage() {
  analogSetAttenuation(ADC_11db); 
  uint32_t rawSum = 0;
  constexpr int samples = 10;
  
  for (int i = 0; i < samples; i++) {
    uint16_t reading = analogRead(BATT_PIN);
    rawSum += reading;
  }
  
  float rawAverage = (float)rawSum / samples;
  float voltage = (rawAverage / 4095.0) * 3.3 * 2.0;
  return voltage * BATTERY_CALIBRATIONFACTOR;
}
