#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "ntfy_cert.h"
#include "ntfy.h"
#include "commands.h"

static const String REPLY_TOPIC = String(NTFY_TOPIC) + "-status";

static String lastMessageId = "";
static bool firstPoll = true;

void publishNtfy(const String& text){
  WiFiClientSecure client;
  client.setCACert(NTFY_ROOT_CA);

  HTTPClient http;
  String url = "https://ntfy.sh/" + String(REPLY_TOPIC);

  http.begin(client, url);
  http.setTimeout(10000);

  int code = http.POST(text);

    if(code != 200){
    Serial.print("ERROR CODE NOT 200: ");
    Serial.println(code);
    http.end();
    return;
  }

  http.end();

}

void pollNtfy(){
  WiFiClientSecure client;
  client.setCACert(NTFY_ROOT_CA);

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
  http.setTimeout(10000);
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
