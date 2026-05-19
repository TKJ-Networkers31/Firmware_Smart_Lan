#pragma once

// ================================================================
// CONFIG.H  —  Semua pin, konstanta, dan konfigurasi jaringan
// Untuk ubah konfigurasi hardware / jaringan, edit file ini saja.
// ================================================================

// -------- PIN --------
#define PIN_RELAY_LAMP1_2   14
#define PIN_RELAY_LAMP3_4   27
#define PIN_SELENOID        17
#define PIN_TRIG            25
#define PIN_ECHO            26
#define PIN_DHT              4
#define PIN_BUZZER          32
#define PIN_LDR             35
#define PIN_RFID_SS          5
#define PIN_RFID_RST        16

// -------- SENSOR CONFIG --------
#define DHT_TYPE            DHT11
#define LDR_THRESHOLD       50       // di bawah nilai ini → gelap → lampu nyala
#define DOOR_TRIGGER_CM     10.0f    // jarak (cm) untuk buka pintu otomatis

// -------- TIMING (ms) --------
#define INTERVAL_DHT        2000UL
#define INTERVAL_PUBLISH    10000UL
#define INTERVAL_TTL        6000000UL
#define INTERVAL_DISTANCE   500UL
#define INTERVAL_WIFI_CHECK 5000UL
#define INTERVAL_DOOR_OPEN  5000UL   // durasi pintu terbuka sebelum otomatis tutup

// -------- WIFI --------
#define WIFI_SSID           "redmi 9c"
#define WIFI_PASS           "11117994"
#define WIFI_RETRY_MAX      20
#define WIFI_RETRY_DELAY_MS 300

// -------- MQTT --------
#define MQTT_SERVER         "192.168.43.93"
#define MQTT_PORT           1883
#define MQTT_CLIENT_ID      "esp32_smartlab_1"
#define MQTT_BUFFER_SIZE    1024
#define MQTT_RETRY_MAX      5

// Publish topics
#define TOPIC_PUB_SENSOR    "lab1/sensor"
#define TOPIC_PUB_ACCESS    "lab1/access"
#define TOPIC_PUB_TTL       "lab1/timetolive"

// Subscribe topics
#define TOPIC_SUB_LOGIN     "lab1/control/login"
#define TOPIC_SUB_MODE      "lab1/control/mode"
#define TOPIC_SUB_LOCKED    "lab1/control/locked"
#define TOPIC_SUB_DOOR      "lab1/control/door"
#define TOPIC_SUB_LAMP1_2   "lab1/control/lamp1_2"
#define TOPIC_SUB_LAMP3_4   "lab1/control/lamp3_4"

// -------- GOOGLE SHEETS --------
#define SHEET_URL \
  "https://script.google.com/macros/s/" \
  "AKfycbz-zqtx90oHWwHfh-QBQ4vu1HvO1AwoGksLby7PP32SADF9nH6_Z1k-1K2RH75wzUJ64A/exec"
#define SHEET_TIMEOUT_MS    5000

// -------- BUFFER SIZES --------
#define JSON_BUF_SIZE       1024
#define USER_NAME_SIZE      16
#define USER_ID_SIZE        64
#define UID_STR_SIZE        32
