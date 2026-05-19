// ================================================================
// Smart Lab — ESP32 IoT Controller
// main.cpp — Entry point (setup & loop)
//
// Semua logika ada di include/SmartLab.h dan komponen-komponennya.
// Untuk ubah pin/config → include/Config.h
// Untuk tambah fitur → buat class baru, include di SmartLab.h
// ================================================================

#include <Arduino.h>
#include "SmartLab.h"

SmartLab lab;

void setup() {
  lab.begin();
}

void loop() {
  lab.update();
}
