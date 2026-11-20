#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WakeOnLan.h>
#include <ESP32Ping.h>
#include "secrets.h"
#include "pc.h"
#include "ntfy.h"
#include "plug.h"
#include <Preferences.h>

enum PcState{ IDLE, WAKING, RETRYING, POWER_CYCLING, BOOTING_AFTER_CYCLE };

static const unsigned long PC_CHECK_INTERVAL_MS = 10000;
static const unsigned long WAKE_TIMEOUT_MS = 120000;
static const unsigned long RETRY_TIMEOUT_MS = 120000;
static const unsigned long PLUG_OFF_SECONDS = 15;
static const unsigned long CYCLE_RESTORE_WAIT_MS = 25000;
static const unsigned long BOOT_AFTER_CYCLE_TIMEOUT_MS = 180000;
static const unsigned long MIN_HOURS_BETWEEN_CYCLES = 6;

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
    case POWER_CYCLING: return "POWER_CYCLING";
    case BOOTING_AFTER_CYCLE: return "BOOTING_AFTER_CYCLE";
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

static bool isApolloUp(){
  WiFiClient c;
  return c.connect(PC_IP, 47989, 2000);
}

static bool cycleAllowed(){
  time_t now = time(nullptr);
  if (now < 1700000000) return false;
  
  Preferences p;
  p.begin("pcremote", true);
  unsigned long lastCycle = p.getULong("lastCycle", 0);
  p.end();
  return now - lastCycle >= MIN_HOURS_BETWEEN_CYCLES *3600;
}

static void rememberCycle(){
  time_t now = time(nullptr);

  Preferences p;
  p.begin("pcremote", false);
  p.putULong("lastCycle", now);
  p.end();
}

void requestPowerCycle(){
  if(pcState != IDLE){
    Serial.println("Power-cycle refused: a wake or cycle is already running");
    publishNtfy("Power-cycle refused: a wake or cycle is already running");
    return;
  }

  if(!cycleAllowed()){
    Serial.println("Power-cycle refused: too soon or no time sync");
    publishNtfy("Power-cycle refused: too soon or no time sync");
    return;
  }

  if(isPcUp() || isApolloUp()){
    Serial.println("Power-cycle refused: PC is responding");
    publishNtfy("Power-cycle refused: PC is responding");
    return;
  }

  if(!plugOffFor(PLUG_OFF_SECONDS)){
    Serial.println("Power-cycle refused: Couldn't reach the plug");
    publishNtfy("Power-cycle refused: Couldn't reach the plug");
    return;
  }

  rememberCycle();
  Serial.println("Power-cycling PC...");
  publishNtfy("Power-cycling PC...");
  setPcState(POWER_CYCLING);
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

  unsigned long inState = millis() - stateEnteredAt;

  if(pcState == POWER_CYCLING){
    
    if(inState < CYCLE_RESTORE_WAIT_MS) return;

    bool on = false; 

    if(!plugGetPower(on) || !on){
      plugSetPower(true);
      plugGetPower(on);
    }

    if(!on){
      Serial.println("Plug stuck OFF, check manually!");
      publishNtfy("Plug stuck OFF, check manually!");
      setPcState(IDLE);
      return; 
    }

    Serial.println("Power is back, waiting for PC to boot...");
    publishNtfy("Power is back, waiting for PC to boot...");
    setPcState(BOOTING_AFTER_CYCLE);
    return; 
  }

  if(isPcUp()){
    Serial.println("PC is up!");
    publishNtfy("PC is up!");
    setPcState(IDLE);
    return;
  }


  if(pcState == WAKING && inState >= WAKE_TIMEOUT_MS){
    Serial.println("No answer after two min, sending wake again...");
    publishNtfy("No answer after two min, sending wake again...");
    WOL.sendMagicPacket(PC_MAC);
    setPcState(RETRYING);
  } else if(pcState == RETRYING && inState >= RETRY_TIMEOUT_MS){
    Serial.println("PC did not respond after 2 tries. Send 'powercycle <PIN>' to force a restart.");
    publishNtfy("PC did not respond after 2 tries. Send 'powercycle <PIN>' to force a restart.");
    setPcState(IDLE);
  } else if(pcState == BOOTING_AFTER_CYCLE && inState >= BOOT_AFTER_CYCLE_TIMEOUT_MS){
    Serial.println("PC still down after power-cycle. Needs manual intervention.");
    publishNtfy("PC still down after power-cycle. Needs manual intervention.");
    setPcState(IDLE);
  }


}
