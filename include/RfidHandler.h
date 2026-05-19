#pragma once
#include <SPI.h>
#include <MFRC522.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "AccessManager.h"

// ================================================================
// RfidHandler  —  Baca kartu RFID dan kirim request login/logout
//
// Logika:
//  - Belum login → tap kartu → publish "login" ke TOPIC_PUB_ACCESS
//  - Sudah login  → tap kartu YANG SAMA → publish "logout"
//  - Sudah login  → tap kartu BERBEDA → tolak (Serial log saja)
// ================================================================

class RfidHandler {
public:
  RfidHandler(AccessManager& access)
    : _rfid(PIN_RFID_SS, PIN_RFID_RST), _access(access) {}

  void begin() {
    SPI.begin();
    _rfid.PCD_Init();
    Serial.println(F("[RFID] Init OK"));
  }

  // Dipanggil setiap loop() saat modeAuto && !locked
  // Mengembalikan true jika ada kartu yang diproses
  bool update(PubSubClient* mqtt, char* jsonBuf, size_t bufSize) {
    if (!mqtt) return false;
    if (!_rfid.PICC_IsNewCardPresent()) return false;
    if (!_rfid.PICC_ReadCardSerial())   return false;

    char uid[UID_STR_SIZE] = "";
    _buildUidStr(uid, sizeof(uid));
    Serial.printf("[RFID] Card: %s\n", uid);

    if (!_access.isLoggedIn()) {
      _publishRequest(mqtt, jsonBuf, bufSize, uid, "login");

    } else if (_access.isCurrentUser(uid)) {
      _publishRequest(mqtt, jsonBuf, bufSize, uid, "logout");

    } else {
      // Kartu berbeda saat sudah ada yang login
      Serial.println(F("[RFID] Card mismatch — ignored"));
    }

    _rfid.PICC_HaltA();
    _rfid.PCD_StopCrypto1();
    return true;
  }

private:
  MFRC522 _rfid;
  AccessManager& _access;

  void _buildUidStr(char* out, size_t outSize) {
    out[0] = '\0';
    for (byte i = 0; i < _rfid.uid.size; i++) {
      char hex[5];
      snprintf(hex, sizeof(hex), i > 0 ? " %02X" : "%02X", _rfid.uid.uidByte[i]);
      strncat(out, hex, outSize - strlen(out) - 1);
    }
    // Uppercase
    for (char* p = out; *p; p++) *p = toupper((unsigned char)*p);
  }

  void _publishRequest(PubSubClient* mqtt, char* buf, size_t bufSize,
                       const char* uid, const char* status) {
    StaticJsonDocument<256> doc;
    doc["uid"]    = uid;
    doc["device"] = MQTT_CLIENT_ID;
    doc["status"] = status;
    size_t n = serializeJson(doc, buf, bufSize);
    if (mqtt->publish(TOPIC_PUB_ACCESS, (const uint8_t*)buf, n, false)) {
      Serial.printf("[RFID] Published %s for %s\n", status, uid);
    } else {
      Serial.println(F("[RFID] Publish failed"));
    }
  }
};
