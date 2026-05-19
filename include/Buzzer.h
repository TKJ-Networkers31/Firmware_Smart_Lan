#pragma once
#include <Arduino.h>

// ================================================================
// Buzzer  —  Non-blocking buzzer manager
//
// Mendukung antrian beep sederhana agar beep tidak saling tumpang
// tindih. Panggil update() setiap loop().
// ================================================================

class Buzzer {
public:
  Buzzer(uint8_t pin) : _pin(pin), _endTime(0), _active(false) {}

  void begin() {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
  }

  // Mulai beep selama durationMs millisecond (non-blocking)
  // Jika sedang beep, request baru diabaikan (tidak tumpang tindih)
  void beep(unsigned int durationMs) {
    if (_active) return; // sedang beep, skip
    digitalWrite(_pin, HIGH);
    _endTime = millis() + durationMs;
    _active = true;
  }

  // Dipanggil setiap loop()
  void update() {
    if (_active && millis() >= _endTime) {
      digitalWrite(_pin, LOW);
      _active = false;
    }
  }

  bool isActive() const { return _active; }

private:
  uint8_t _pin;
  unsigned long _endTime;
  bool _active;
};
