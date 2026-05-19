#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "Config.h"
#include "LcdManager.h"
#include "Buzzer.h"
#include "DoorController.h"
#include "SensorManager.h"
#include "RelayManager.h"
#include "AccessManager.h"
#include "NetworkManager.h"
#include "RfidHandler.h"
#include "SheetLogger.h"

// ================================================================
// SmartLab  —  Koordinator utama seluruh sistem
//
// Semua logika aplikasi (auto mode, manual mode, lock, login, lcd)
// ada di sini. Komponen lain hanya menyediakan layanan.
// ================================================================

class SmartLab {
public:
  SmartLab()
    : _lcd(0x27, 16, 2),
      _buzzer(PIN_BUZZER),
      _door(PIN_SELENOID, _buzzer),
      _relay(),
      _access(),
      _network(_lcd),
      _rfid(_access),
      _sheet(),
      _lastPub(0), _lastTtl(0),
      _lastLoginState(false), _lastLockedState(false) {
    memset(_jsonBuf, 0, sizeof(_jsonBuf));
  }

  // ----------------------------------------------------------
  // Harus dipanggil dari setup()
  // ----------------------------------------------------------
  void begin() {
    Serial.begin(115200);
    delay(500);
    Serial.printf("\n[SYS] Free heap: %u bytes\n", ESP.getFreeHeap());

    // LCD pertama kali
    _lcd.begin();
    _lcd.show("Smart Lab", "Booting...");

    // Hardware
    _buzzer.begin();
    _door.begin();
    _relay.begin();
    _sensors.begin();
    _rfid.begin();

    delay(500);

    // Network (WiFi + MQTT)
    // Callback MQTT harus static/free function, kita bridge lewat pointer
    _instance = this;
    _network.begin(_wifiClient, _mqttCallback);

    delay(500);

    // Tampilan awal
    _lcd.setDefault("Silahkan", "Login");
    _lcd.show("Silahkan", "Login");
    _buzzer.beep(300);

    Serial.println(F("[SYS] === Smart Lab Ready ==="));
  }

  // ----------------------------------------------------------
  // Harus dipanggil dari loop()
  // ----------------------------------------------------------
  void update() {
    unsigned long now = millis();

    // Update semua komponen
    _buzzer.update();
    _door.update();
    _lcd.update();
    _sensors.update();
    _network.update();

    // Publish sensor ke MQTT
    if (now - _lastPub >= INTERVAL_PUBLISH) {
      _lastPub = now;
      _publishStatus();
    }

    // Time-to-live heartbeat
    if (now - _lastTtl >= INTERVAL_TTL) {
      _lastTtl = now;
      _publishTTL();
    }

    // Jalankan logika berdasarkan mode
    if (_access.isLocked()) {
      _handleLockedState();
    } else if (_access.isModeAuto()) {
      _handleAutoMode();
    }
    // Manual mode: kontrol sepenuhnya dari MQTT callback
    // tidak ada logic tambahan di loop
  }

private:
  // -------- Komponen --------
  LcdManager      _lcd;
  Buzzer          _buzzer;
  DoorController  _door;
  SensorManager   _sensors;
  RelayManager    _relay;
  AccessManager   _access;
  NetworkManager  _network;
  RfidHandler     _rfid;
  SheetLogger     _sheet;
  WiFiClient      _wifiClient;

  // -------- State --------
  char          _jsonBuf[JSON_BUF_SIZE];
  unsigned long _lastPub, _lastTtl;
  bool          _lastLoginState, _lastLockedState;

  // Pointer static untuk bridge MQTT callback (C callback tidak bisa lambda)
  static SmartLab* _instance;

  // ----------------------------------------------------------
  // AUTO MODE
  // ----------------------------------------------------------
  void _handleAutoMode() {
    // Proses RFID tap
    _rfid.update(_network.getMqtt(), _jsonBuf, sizeof(_jsonBuf));

    // Update LCD hanya jika state login berubah
    bool loginNow = _access.isLoggedIn();
    if (loginNow != _lastLoginState) {
      _lastLoginState = loginNow;
      if (loginNow) {
        // BUG FIX: di kode lama, pesan "Akses Diterima" di sini
        // langsung ditimpa pesan default di iterasi loop berikutnya.
        // Sekarang default diset dulu ke nama user, baru restore.
        char line2[17];
        snprintf(line2, sizeof(line2), "Hi, %.13s!", _access.getUser());
        _lcd.setDefault("Akses Diterima", line2);
        _lcd.restoreDefault();
      } else {
        _relay.allOff();
        _door.forceClose();
        _lcd.setDefault("Silahkan", "Login");
        _lcd.restoreDefault();
      }
    }

    if (_access.isLoggedIn()) {
      // Auto lampu berdasarkan LDR
      _relay.setLamp1_2(_sensors.isDark());
      _relay.setLamp3_4(_sensors.isDark());

      // Auto buka pintu jika ada yang mendekat
      if (_sensors.isNearDoor()) {
        _door.open();
      }
    }
  }

  // ----------------------------------------------------------
  // LOCKED STATE
  // ----------------------------------------------------------
  void _handleLockedState() {
    bool lockedNow = _access.isLocked();
    if (lockedNow != _lastLockedState) {
      _lastLockedState = lockedNow;
      if (lockedNow) {
        _lcd.setDefault("Ruangan", "Dikunci");
        _lcd.restoreDefault();
      }
    }
  }

  // ----------------------------------------------------------
  // MQTT PUBLISH
  // ----------------------------------------------------------
  void _publishStatus() {
    StaticJsonDocument<1024> doc;

    doc["device"]   = MQTT_CLIENT_ID;
    doc["modeAuto"] = _access.isModeAuto();
    doc["locked"]   = _access.isLocked();
    doc["login"]    = _access.isLoggedIn();
    doc["user"]     = _access.getUser();
    doc["uid"]      = _access.getUID();

    if (_network.isWifiConnected()) {
      char ipStr[16];
      IPAddress ip = WiFi.localIP();
      snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      doc["ip"]   = ipStr;
      doc["rssi"] = WiFi.RSSI();
    }

    doc["temp"]       = _sensors.getTemp();
    doc["hum"]        = _sensors.getHumidity();
    doc["light"]      = _sensors.getLDR();
    doc["distance"]   = _sensors.getDistance();
    doc["freeMemory"] = ESP.getFreeHeap() / 1024;
    doc["maxAlloc"]   = ESP.getMaxAllocHeap() / 1024;
    doc["door"]       = _door.isOpen();
    doc["lamp1_2"]    = _relay.getLamp1_2();
    doc["lamp3_4"]    = _relay.getLamp3_4();

    size_t n = serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    if (_network.publish(TOPIC_PUB_SENSOR, _jsonBuf, n)) {
      Serial.printf("[PUB] Sensor OK (%u bytes)\n", n);
    } else {
      Serial.println(F("[PUB] Sensor FAILED"));
    }
  }

  void _publishTTL() {
    StaticJsonDocument<128> doc;
    doc["device"]  = MQTT_CLIENT_ID;
    doc["message"] = "hello";
    size_t n = serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    _network.publish(TOPIC_PUB_TTL, _jsonBuf, n);
    Serial.println(F("[PUB] TTL sent"));
  }

  // ----------------------------------------------------------
  // MQTT CALLBACK (static bridge)
  // ----------------------------------------------------------
  static void _mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance) _instance->_handleMqttMessage(topic, payload, length);
  }

  void _handleMqttMessage(char* topic, byte* payload, unsigned int length) {
    if (!topic || !payload || length == 0) return;

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
      Serial.printf("[MQTT] JSON error: %s\n", err.c_str());
      return;
    }

    Serial.printf("[MQTT] Topic: %s\n", topic);

    // ---- lab1/control/login → hasil auth dari server/broker ----
    if (strcmp(topic, TOPIC_SUB_LOGIN) == 0 && !_access.isLocked()) {
      const char* sAccess = doc["statusAccess"] | "denied";
      const char* uName   = doc["user"]         | "none";
      const char* uuid    = doc["uid"]          | "none";

      if (strcmp(sAccess, "success") == 0) {
        if (!_access.isLoggedIn()) {
          // ---- LOGIN ----
          _access.doLogin(uName, uuid);
          _door.open();
          _sheet.send(uuid, uName, "login");

          // BUG FIX: Pesan login tampil 3 detik, lalu otomatis
          // kembali ke "Akses Diterima / Hi, <nama>"
          // Di kode lama, tidak ada timed display → pesan langsung
          // hilang di iterasi loop berikutnya
          char line2[17];
          snprintf(line2, sizeof(line2), "Hi, %.13s!", uName);
          _lcd.showTimed("Akses Diterima", line2, 3000);
          _lcd.setDefault("Akses Diterima", line2);
          _buzzer.beep(300);

        } else {
          // ---- LOGOUT ----
          // (Dipicu oleh tap kartu yang sama, server kirim success)
          _sheet.send(_access.getUID(), _access.getUser(), "logout");
          _access.doLogout();
          _relay.allOff();
          _door.open(); // buka pintu sekali saat logout

          // BUG FIX: showTimed → setelah 3 detik kembali ke "Silahkan Login"
          _lcd.showTimed("Sampai Jumpa!", "Bye!", 3000);
          _lcd.setDefault("Silahkan", "Login");
          _buzzer.beep(200);
        }

      } else {
        // ---- AKSES DITOLAK ----
        _sheet.send(uuid, "tidak dikenal", "denied");

        // BUG FIX: di kode lama, showTimed tidak ada — pesan
        // "Akses Ditolak" hilang seketika karena loop langsung
        // overwrite LCD. Sekarang tampil 3 detik lalu kembali.
        _lcd.showTimed("Akses Ditolak", "Kartu Salah", 3000);
        _buzzer.beep(1000);
        Serial.println(F("[ACCESS] Denied"));
      }
    }

    // ---- lab1/control/mode ----
    else if (strcmp(topic, TOPIC_SUB_MODE) == 0) {
      bool autoMode = doc["mode_auto"] | true;
      _access.setModeAuto(autoMode);
      if (!_access.isLoggedIn()) {
        _relay.allOff();
        _door.forceClose();
      }
      Serial.printf("[MQTT] Mode: %s\n", autoMode ? "Auto" : "Manual");
    }

    // ---- lab1/control/locked ----
    else if (strcmp(topic, TOPIC_SUB_LOCKED) == 0 && !_access.isLoggedIn()) {
      bool lockCmd = doc["locked"] | false;
      _access.setLocked(lockCmd);
      if (lockCmd) {
        _relay.allOff();
        _door.forceClose();
        _lcd.setDefault("Ruangan", "Dikunci");
        _lcd.showTimed("Ruangan", "Dikunci", 0); // tampil permanen
        _lcd.restoreDefault();
      } else {
        _lcd.setDefault("Silahkan", "Login");
        _lcd.restoreDefault();
      }
      Serial.printf("[MQTT] Locked: %s\n", lockCmd ? "YES" : "NO");
    }

    // ---- lab1/control/door (manual mode only) ----
    else if (strcmp(topic, TOPIC_SUB_DOOR) == 0
             && !_access.isLocked() && !_access.isModeAuto()) {
      bool doorVal = doc["door"] | false;
      if (doorVal) _door.open();
      else         _door.forceClose();
    }

    // ---- lab1/control/lamp1_2 (manual mode only) ----
    else if (strcmp(topic, TOPIC_SUB_LAMP1_2) == 0
             && !_access.isLocked() && !_access.isModeAuto()) {
      _relay.setLamp1_2(doc["lampu1_2"] | false);
    }

    // ---- lab1/control/lamp3_4 (manual mode only) ----
    else if (strcmp(topic, TOPIC_SUB_LAMP3_4) == 0
             && !_access.isLocked() && !_access.isModeAuto()) {
      _relay.setLamp3_4(doc["lampu3_4"] | false);
    }
  }
};

// Definisi static member (satu per translation unit)
SmartLab* SmartLab::_instance = nullptr;
