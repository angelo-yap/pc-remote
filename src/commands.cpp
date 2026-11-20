#include <Arduino.h>
#include "commands.h"
#include "ntfy.h"
#include "pc.h"
#include "led.h"
#include "secrets.h"

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
  }else if(formattedMessage == "status") {
    bool up = isPcUp();
    String reply = up ? "PC is up..." : "PC is down...";

    Serial.println(reply);
    publishNtfy(reply);
    blinkLed(4);
  }else if(formattedMessage.startsWith("powercycle ")){
    String pin = formattedMessage.substring(11);
    
    if(pin == POWERCYCLE_PIN){
      requestPowerCycle();
    }else{
      Serial.println("Wrong PIN");
      publishNtfy("Wrong PIN");
    }
    blinkLed(3);
  }else{
    Serial.println("Unknown Command");
    blinkLed(2);
  }
}
