#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"

const unsigned long WIFI_TIMEOUT_MS = 20000;

void connectWiFi(){
  
  WiFi.mode(WIFI_STA);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
  Serial.println("Connecting to Wi-Fi...");
  unsigned long start = millis(); 

  while(WiFi.status() != WL_CONNECTED) {

    unsigned long current = millis();

    if(current - start > WIFI_TIMEOUT_MS){
      Serial.print("ERROR CONNECTING TO ");
      Serial.println(WIFI_SSID);
      ESP.restart();
    }

    Serial.print(".");
    delay(500);
  }
  Serial.print("\nConnected! IP: ");
  Serial.println(WiFi.localIP());
}


void setup(){

  Serial.begin(115200);
  delay(1000);
  connectWiFi();
}

void loop(){
  
}