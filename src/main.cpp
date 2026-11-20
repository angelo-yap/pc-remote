#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include "led.h"
#include "wifi_conn.h"
#include "ntfy.h"
#include "pc.h"

const unsigned long POLL_INTERVAL_MS = 5000;
const uint32_t WDT_TIMEOUT_MS = 30000;

unsigned long lastPoll = 0;
bool wifiWasDown = false;

void setup(){

  Serial.begin(115200);
  delay(1000);
  setupLed();
  connectWiFi();

  setupWol();

  //watchdog
  esp_task_wdt_config_t cfg = {
    .timeout_ms = WDT_TIMEOUT_MS,
    .idle_core_mask = 0,
    .trigger_panic = true
  };

  esp_task_wdt_reconfigure(&cfg);
  esp_task_wdt_add(NULL);
}

void loop(){
  esp_task_wdt_reset();

  if(!ensureWiFi()){
    wifiWasDown = true;
    delay(1000);
    return;
  }

  if(wifiWasDown){
    Serial.print("Wi-Fi is back! IP: ");
    Serial.println(WiFi.localIP());
    updateWolBroadcast();
    wifiWasDown = false;
  }


  if (millis() - lastPoll >= POLL_INTERVAL_MS) {
    lastPoll = millis();
    pollNtfy();
  }

  checkPcBoot();
}
