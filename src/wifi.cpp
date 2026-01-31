/**
 * @copybrief wifi.hpp
 * @author Aya Kraise
 */

#include <WiFi.h>

#include "wifi.hpp"
#include "cred.hpp"

using namespace dashboard;

void dashboard::connectWifi(void) {
  Serial.printf("Attemping to connect with %s %s\n", WIFI_CREDS.WifiName.c_str(), WIFI_CREDS.Password.c_str());

  WiFi.begin(WIFI_CREDS.WifiName.c_str(), WIFI_CREDS.Password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  
  Serial.printf("CONNECTED, IP address: %s\n", WiFi.localIP().toString().c_str());
}
