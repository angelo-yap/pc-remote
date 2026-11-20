#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WakeOnLan.h>
#include <ESP32Ping.h>
#include "secrets.h"
#include "pc.h"
#include "ntfy.h"

enum PcState{ IDLE, WAKING, RETRYING };

static const unsigned long PC_CHECK_INTERVAL_MS = 10000;
static const unsigned long WAKE_TIMEOUT_MS = 120000;
static const unsigned long RETRY_TIMEOUT_MS = 120000;

static PcState pcState = IDLE;
static unsigned long stateEnteredAt = 0;
static unsigned long lastPcCheck = 0;

static WiFiUDP UDP;
static WakeOnLan WOL(UDP);

static const char* stateName(PcState s){
  switch(s){
    case IDLE: return "IDLE";
    case WAKING: return "WAKING";
    case RETRYING: return "RETRYING";
  }
  return "UNKNOWN";
}

static void setPcState(PcState next){
  pcState = next;
  stateEnteredAt = millis();
  lastPcCheck = millis();
  Serial.print("New PC state entered: ");
  Serial.println(stateName(next));
}

void setupWol(){
  updateWolBroadcast();
  WOL.setRepeat(3, 100); // 3 packets sent for redundancy
}

// Call again after reconnecting, the IP may have changed
void updateWolBroadcast(){
  WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask());
}

void sendWake(){
  if(pcState != IDLE){
    Serial.println("Already waking...");
    publishNtfy("Already waking...");
    return;
  }

  WOL.sendMagicPacket(PC_MAC);
  Serial.print("Sent Magic Packet to: ");
  Serial.println(PC_MAC);

  setPcState(WAKING);
}

bool isPcUp(){
  IPAddress pcIp;

  pcIp.fromString(PC_IP);
  return Ping.ping(pcIp, 3);
}

// After a wake, ping every 10 s and notify once the PC answers
void updatePcState(){
  if(pcState == IDLE) return;
  if(millis() - lastPcCheck < PC_CHECK_INTERVAL_MS) return;
  lastPcCheck = millis();

  if(isPcUp()){
    Serial.println("PC is up!");
    publishNtfy("PC is up!");
    setPcState(IDLE);
    return;
  }

  unsigned long inState = millis() - stateEnteredAt;

  if(pcState == WAKING && inState >= WAKE_TIMEOUT_MS){
    Serial.println("No answer after two min, sending wake again...");
    publishNtfy("No answer after two min, sending wake again...");
    WOL.sendMagicPacket(PC_MAC);
    setPcState(RETRYING);
  }else if(pcState == RETRYING && inState >= RETRY_TIMEOUT_MS){
    Serial.println("PC did not respond after 2 tries...");
    publishNtfy("PC did not respond after 2 tries...");
    setPcState(IDLE);

    //TODO POWER CYCLE WITH SMART PLUG
  }


}
