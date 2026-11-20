#include <Arduino.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include "ota.h"

// OTA_PASSWORD comes from secrets.ini through a build flag (see platformio.ini)
#ifndef OTA_PASSWORD
#error "OTA_PASSWORD not set: copy secrets.example.ini to secrets.ini"
#endif

static const char* OTA_HOSTNAME = "pc-remote";

void setupOta(){
  ArduinoOTA.setHostname(OTA_HOSTNAME);   // reachable as pc-remote.local
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([](){
    Serial.println("OTA update starting...");
  });

  // An upload takes longer than the watchdog timeout, so keep feeding it
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total){
    esp_task_wdt_reset();
  });

  ArduinoOTA.onEnd([](){
    Serial.println("OTA update done, rebooting");
  });

  ArduinoOTA.onError([](ota_error_t err){
    Serial.print("OTA error: ");
    Serial.println(err);
  });

  ArduinoOTA.begin();
}

void handleOta(){
  ArduinoOTA.handle();
}
