#pragma once
#include <Arduino.h>
#include "Buzzer.h"
#include "Config.h"

// ================================================================
// DoorController  —  Solenoid door lock dengan auto-close timer
// ================================================================

class DoorController {
public:
  DoorController(uint8_t pin, Buzzer& buzzer)
    : _pin(pin), _buzzer(buzzer), _isOpen(false), _openedAt(0) {}

  void begin() {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
  }

  // Buka pintu (idempoten — kalau sudah buka, tidak melakukan apa-apa)
  void open() {
    if (_isOpen) return;
    digitalWrite(_pin, HIGH);
    _buzzer.beep(200);
    _isOpen = true;
    _openedAt = millis();
    Serial.println(F("[DOOR] Opened"));
  }

  // Tutup paksa (misalnya saat logout / lock)
  void forceClose() {
    if (!_isOpen) return;
    digitalWrite(_pin, LOW);
    _isOpen = false;
    Serial.println(F("[DOOR] Force closed"));
  }

  // Dipanggil setiap loop() — auto-close setelah INTERVAL_DOOR_OPEN ms
  void update() {
    if (_isOpen && (millis() - _openedAt >= INTERVAL_DOOR_OPEN)) {
      digitalWrite(_pin, LOW);
      _buzzer.beep(200);
      _isOpen = false;
      Serial.println(F("[DOOR] Auto-closed"));
    }
  }

  bool isOpen() const { return _isOpen; }

private:
  uint8_t _pin;
  Buzzer& _buzzer;
  bool _isOpen;
  unsigned long _openedAt;
};
