#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <WakeOnLan.h>
#include <ESP32Ping.h>

const int LED_PIN = 2;

const String REPLY_TOPIC = String(NTFY_TOPIC) + "-status";

const unsigned long WIFI_TIMEOUT_MS = 20000;
const unsigned long POLL_INTERVAL_MS = 5000;

String lastMessageId = "";
bool firstPoll = true;
unsigned long lastPoll = 0;

WiFiUDP UDP;
WakeOnLan WOL(UDP);

bool isPcUp(){
  IPAddress pcIp;

  pcIp.fromString(PC_IP);
  return Ping.ping(pcIp, 3); 
}

void blinkLed(int times){
  for(int i = 0; i < times; i++){
    digitalWrite(LED_PIN,HIGH);
    delay(100);
    digitalWrite(LED_PIN,LOW);
    delay(100);     
  }
}

void publishNtfy(const String& text){
  WiFiClientSecure client;
  client.setInsecure(); //TODO certificate check

  HTTPClient http;
  String url = "https://ntfy.sh/" + String(REPLY_TOPIC);

  http.begin(client, url);

  int code = http.POST(text);

    if(code != 200){
    Serial.print("ERROR CODE NOT 200: ");
    Serial.println(code);
    http.end();
    return;
  }

  http.end();
  
}

void handleCommand(const String& msg){
  Serial.print("Received command: ");
  Serial.println(msg);  

  // In case "Wake" or "wake "
  String formattedMessage = msg;
  formattedMessage.trim();
  formattedMessage.toLowerCase();

  if(formattedMessage == "wake"){
    WOL.sendMagicPacket(PC_MAC);
    Serial.print("Sent Magic Packet to: ");
    Serial.println(PC_MAC);
    blinkLed(6);
  } else if(formattedMessage == "status") {
    bool up = isPcUp(); 
    String reply = up ? "PC is up..." : "PC is down...";

    Serial.println(reply);
    publishNtfy(reply);
    blinkLed(4);
  } else {
    Serial.println("Unknown Command");
    blinkLed(2);
  }
}

void pollNtfy(){
  WiFiClientSecure client;
  client.setInsecure(); //TODO certificate check

  HTTPClient http;
  String url = "https://ntfy.sh/" + String(NTFY_TOPIC) + "/json?poll=1&since=";

  if (firstPoll){
    url += "latest";
  } else if (lastMessageId == ""){
    url += "all";
  } else {
    url += lastMessageId;
  }

  http.begin(client, url);

  int code = http.GET();

  if(code != 200){
    Serial.print("ERROR CODE NOT 200: ");
    Serial.println(code);
    http.end();
    return;
  }

  String body = http.getString();
  http.end();
  
  int start = 0;
  while (start < body.length()){
    int end = body.indexOf('\n', start);
    if (end == -1) end = body.length();

    String line = body.substring(start, end);
    start = end + 1;

    if (line.length() == 0) continue;

    // Parse & error check
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, line); 
    if(err){
      Serial.print("ERROR PARSING: ");
      Serial.println(err.c_str());
      continue;
    }

    // Only allow messages
    if (doc["event"] == "message") {
      lastMessageId = doc["id"].as<String>();

      // Only handle the messages after the first one.
      if (!firstPoll) {
        handleCommand(doc["message"].as<String>());
      }
    }
  }

  firstPoll = false;
}

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
  pinMode(LED_PIN, OUTPUT);
  connectWiFi();

  WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask());
  WOL.setRepeat(3, 100); // 3 packets sent for redundancy
}

void loop(){ 
  if (millis() - lastPoll >= POLL_INTERVAL_MS) {
    lastPoll = millis();
    pollNtfy();
  }
}