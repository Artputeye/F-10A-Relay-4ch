#include <Arduino.h>
#include "relay_control.h"
#include "motor_bts7960.h"

void setup() {
  Serial.begin(115200);

  relaySetup();
  motorSetup();
}

void loop() {

  relayLoop();
  motorLoop();

  delay(10);
}