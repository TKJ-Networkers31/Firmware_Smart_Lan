#pragma once
#include <Arduino.h>
#include <DHT.h>
#include "Config.h"

// ================================================================
// SensorManager  —  Handle DHT11, LDR, dan Ultrasonic HC-SR04
//
// Semua pembacaan non-blocking berbasis millis().
// Panggil update() setiap loop().
// ================================================================

class SensorManager {
public:
  SensorManager()
    : _dht(PIN_DHT, DHT_TYPE),
      _lastDHT(0), _lastDistance(0),
      _temp(0.0f), _hum(0.0f), _ldr(0), _distance(0.0f) {}

  void begin() {
    _dht.begin();
    pinMode(PIN_LDR,  INPUT);
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    Serial.println(F("[SENSOR] Init OK"));
  }

  // Dipanggil setiap loop()
  void update() {
    unsigned long now = millis();

    // DHT — baca setiap INTERVAL_DHT
    if (now - _lastDHT >= INTERVAL_DHT) {
      _lastDHT = now;
      float t = _dht.readTemperature();
      float h = _dht.readHumidity();
      if (!isnan(t)) _temp = t;
      if (!isnan(h)) _hum  = h;
      Serial.printf("[SENSOR] T:%.1fC H:%.0f%%\n", _temp, _hum);
    }

    // LDR — baca setiap loop (cepat, analogRead tidak blocking)
    _ldr = analogRead(PIN_LDR);

    // Ultrasonic — baca setiap INTERVAL_DISTANCE
    if (now - _lastDistance >= INTERVAL_DISTANCE) {
      _lastDistance = now;
      _distance = _readUltrasonic();
    }
  }

  // -------- Getter --------
  float    getTemp()     const { return _temp; }
  float    getHumidity() const { return _hum; }
  int      getLDR()      const { return _ldr; }
  float    getDistance() const { return _distance; }
  bool     isDark()      const { return _ldr <= LDR_THRESHOLD; }
  bool     isNearDoor()  const { return _distance > 0 && _distance <= DOOR_TRIGGER_CM; }

private:
  DHT _dht;
  unsigned long _lastDHT;
  unsigned long _lastDistance;
  float _temp, _hum, _distance;
  int _ldr;

  float _readUltrasonic() {
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    long dur = pulseIn(PIN_ECHO, HIGH, 30000);
    if (dur == 0) return -1.0f;
    return (dur / 2.0f) * 0.0343f;
  }
};
