#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WakeOnLan.h>
#include <ESP32Ping.h>
#include "secrets.h"
#include "pc.h"
#include "ntfy.h"

static const unsigned long PC_CHECK_INTERVAL_MS = 10000;
static const unsigned long PC_BOOT_TIMEOUT_MS = 180000;

static bool waitingForPc = false;
static unsigned long wakeSentAt = 0;
static unsigned long lastPcCheck = 0;

static WiFiUDP UDP;
static WakeOnLan WOL(UDP);

void setupWol(){
  updateWolBroadcast();
  WOL.setRepeat(3, 100); // 3 packets sent for redundancy
}

// Call again after reconnecting, the IP may have changed
void updateWolBroadcast(){
  WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask());
}

void sendWake(){
  WOL.sendMagicPacket(PC_MAC);
  Serial.print("Sent Magic Packet to: ");
  Serial.println(PC_MAC);

  waitingForPc = true;
  wakeSentAt = millis();
  lastPcCheck = millis();
}

bool isPcUp(){
  IPAddress pcIp;

  pcIp.fromString(PC_IP);
  return Ping.ping(pcIp, 3);
}

// After a wake, ping every 10 s and notify once the PC answers
void checkPcBoot(){
  if(!waitingForPc) return;
  if(millis() - lastPcCheck < PC_CHECK_INTERVAL_MS) return;
  lastPcCheck = millis();

  if(isPcUp()){
    Serial.println("PC is up!");
    publishNtfy("PC is up!");
    waitingForPc = false;
  } else if(millis() - wakeSentAt >= PC_BOOT_TIMEOUT_MS){
    Serial.println("PC didn't come up after 3 min");
    publishNtfy("PC didn't come up after 3 min...");
    waitingForPc = false;
  }
}
