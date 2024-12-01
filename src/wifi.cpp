#include "cred.h"
#include <WiFi.h>

void connectWifi() {
  Serial.printf("Attemping to connect with %s %s\n", ssid, password);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  
  Serial.println(" CONNECTED");
}
