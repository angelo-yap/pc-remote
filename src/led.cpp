#include <Arduino.h>
#include "led.h"

static const int LED_PIN = 2;

void setupLed(){
  pinMode(LED_PIN, OUTPUT);
}

void blinkLed(int times){
  for(int i = 0; i < times; i++){
    digitalWrite(LED_PIN,HIGH);
    delay(100);
    digitalWrite(LED_PIN,LOW);
    delay(100);
  }
}
