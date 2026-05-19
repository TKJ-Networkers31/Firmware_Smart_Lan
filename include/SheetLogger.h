#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "Config.h"

// ================================================================
// SheetLogger  —  Kirim log akses ke Google Sheets via HTTP GET
// ================================================================

class SheetLogger {
public:
  SheetLogger() {}

  // uid, name, status: "login" / "logout" / "denied"
  void send(const char* uid, const char* name, const char* status) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println(F("[SHEET] WiFi not connected, skip"));
      return;
    }

    char url[512];
    snprintf(url, sizeof(url),
      "%s?uid=%s&name=%s&status=%s",
      SHEET_URL, uid, name, status);

    Serial.printf("[SHEET] Sending: uid=%s name=%s status=%s\n", uid, name, status);

    HTTPClient http;
    http.begin(url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(SHEET_TIMEOUT_MS);

    int code = http.GET();
    if (code > 0) {
      Serial.printf("[SHEET] HTTP %d OK\n", code);
    } else {
      Serial.printf("[SHEET] Error: %d\n", code);
    }
    http.end();
  }
};
