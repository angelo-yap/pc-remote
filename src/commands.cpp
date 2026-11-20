#include <Arduino.h>
#include "commands.h"
#include "ntfy.h"
#include "pc.h"
#include "led.h"

void handleCommand(const String& msg){
  Serial.print("Received command: ");
  Serial.println(msg);

  // In case "Wake" or "wake "
  String formattedMessage = msg;
  formattedMessage.trim();
  formattedMessage.toLowerCase();

  if(formattedMessage == "wake"){
    sendWake();
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
