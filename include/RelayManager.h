#pragma once
#include <Arduino.h>
#include "Config.h"

// ================================================================
// RelayManager  —  Handle relay 2 channel lampu
// ================================================================

class RelayManager {
public:
  RelayManager() {}

  void begin() {
    pinMode(PIN_RELAY_LAMP1_2, OUTPUT);
    pinMode(PIN_RELAY_LAMP3_4, OUTPUT);
    allOff();
    Serial.println(F("[RELAY] Init OK"));
  }

  void setLamp1_2(bool state) {
    digitalWrite(PIN_RELAY_LAMP1_2, state ? HIGH : LOW);
  }

  void setLamp3_4(bool state) {
    digitalWrite(PIN_RELAY_LAMP3_4, state ? HIGH : LOW);
  }

  void allOn()  { setLamp1_2(true);  setLamp3_4(true); }
  void allOff() { setLamp1_2(false); setLamp3_4(false); }

  bool getLamp1_2() const { return digitalRead(PIN_RELAY_LAMP1_2); }
  bool getLamp3_4() const { return digitalRead(PIN_RELAY_LAMP3_4); }
};
