#include "cred.h"
#include <WiFi.h>

void connectWifi() {
  Serial.printf("Attemping to connect with %s %s\n", wifi_credentials::ssid, wifi_credentials::password);

  WiFi.begin(wifi_credentials::ssid, wifi_credentials::password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  
  Serial.println(" CONNECTED");
}
