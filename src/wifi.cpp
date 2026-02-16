/**
 * @copybrief wifi.hpp
 * @author Aya Kraise
 */

#include <WiFi.h>

#include "wifi.hpp"
#include "config.hpp"

using namespace dashboard;

void dashboard::connectWifi(void) {
  Serial.printf("Attemping to connect with %s %s\n", SSID, PASSWORD);
  WiFi.config(LOCAL_IP, GATEWAY, SUBNET, DNS);


  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  
  Serial.printf("CONNECTED, IP address: %s, MAC address: %s\n", WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str());
}

long dashboard::GetRssi(void) {
  long rssi = WiFi.RSSI();
  Serial.printf("Current RSSI: %ld dBm\n", rssi);
  return rssi;
}
