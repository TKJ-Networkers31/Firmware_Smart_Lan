#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "LcdManager.h"

// ================================================================
// NetworkManager  —  Handle WiFi reconnect dan MQTT
//
// MQTT callback di-set dari luar (di SmartLab) agar NetworkManager
// tidak perlu tahu tentang logika aplikasi.
// ================================================================

class NetworkManager {
public:
  NetworkManager(LcdManager& lcd)
    : _lcd(lcd), _lastWifiCheck(0), _lastPub(0), _lastTtl(0) {
    _wifiClient = nullptr;
    _mqtt = nullptr;
  }

  void begin(WiFiClient& wifiClient, MQTT_CALLBACK_SIGNATURE) {
    _wifiClient = &wifiClient;
    _mqtt = new PubSubClient(wifiClient);
    _mqtt->setServer(MQTT_SERVER, MQTT_PORT);
    _mqtt->setBufferSize(MQTT_BUFFER_SIZE);
    _mqtt->setCallback(callback);

    _connectWifi();
    _connectMQTT();
  }

  // Dipanggil setiap loop()
  void update() {
    unsigned long now = millis();

    // WiFi watchdog
    if (now - _lastWifiCheck >= INTERVAL_WIFI_CHECK) {
      _lastWifiCheck = now;
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[NET] WiFi lost, reconnecting..."));
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASS);
      }
    }

    // MQTT reconnect
    if (_mqtt && !_mqtt->connected()) {
      _connectMQTT();
    }

    if (_mqtt) _mqtt->loop();
  }

  // Publish JSON string ke topic
  bool publish(const char* topic, const char* payload, size_t len) {
    if (!_mqtt || !_mqtt->connected()) return false;
    return _mqtt->publish(topic, (const uint8_t*)payload, len, false);
  }

  bool isWifiConnected()  const { return WiFi.status() == WL_CONNECTED; }
  bool isMqttConnected()  const { return _mqtt && _mqtt->connected(); }
  PubSubClient* getMqtt() { return _mqtt; }

private:
  LcdManager&  _lcd;
  WiFiClient*  _wifiClient;
  PubSubClient* _mqtt;
  unsigned long _lastWifiCheck, _lastPub, _lastTtl;

  void _connectWifi() {
    _lcd.show("WiFi...", "");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < WIFI_RETRY_MAX) {
      delay(WIFI_RETRY_DELAY_MS);
      retry++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      IPAddress ip = WiFi.localIP();
      char line1[17], line2[17];
      snprintf(line1, sizeof(line1), "%.16s", WiFi.SSID().c_str());
      snprintf(line2, sizeof(line2), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      _lcd.show(line1, line2);
      Serial.printf("[NET] WiFi OK | IP: %s | RSSI: %d\n", line2, WiFi.RSSI());
    } else {
      _lcd.show("WiFi GAGAL", "Cek SSID/Pass");
      Serial.println(F("[NET] WiFi FAILED"));
    }
  }

  void _connectMQTT() {
    if (!_mqtt) return;

    Serial.println(F("[NET] Connecting MQTT..."));
    int retry = 0;
    while (!_mqtt->connected() && retry < MQTT_RETRY_MAX) {
      if (_mqtt->connect(MQTT_CLIENT_ID)) break;
      delay(500);
      retry++;
    }

    if (_mqtt->connected()) {
      // Subscribe semua topic
      _mqtt->subscribe(TOPIC_SUB_LOGIN);
      _mqtt->subscribe(TOPIC_SUB_MODE);
      _mqtt->subscribe(TOPIC_SUB_LOCKED);
      _mqtt->subscribe(TOPIC_SUB_DOOR);
      _mqtt->subscribe(TOPIC_SUB_LAMP1_2);
      _mqtt->subscribe(TOPIC_SUB_LAMP3_4);
      Serial.println(F("[NET] MQTT connected + subscribed"));
      _lcd.show("MQTT Connected", "");
    } else {
      Serial.println(F("[NET] MQTT FAILED"));
    }
  }
};
