#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPClient.h>

#define SS_PIN 5
#define RST_PIN 22
#define BUZZER 2

MFRC522 rfid(SS_PIN, RST_PIN);

// WIFI
const char* ssid = "GEDUNG-S21@TJKT-SMKN2BE";
const char* pass = "tjkt2025";

// GOOGLE SCRIPT URL
String sheetURL = "https://script.google.com/macros/s/AKfycbyBuyQbx1eeVmAS8pZ4vjCodOl4WxWJDhxrtvyJEfypPvglWlk1EEROIJqoNKbAnC8G/exec";

void sendToSheet(String uid, String name, String status);

void setup()
{
  Serial.begin(115200);

  SPI.begin();
  rfid.PCD_Init();

  WiFi.begin(ssid, pass);

  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");

}

void loop()
{

  if (!rfid.PICC_IsNewCardPresent())
    return;

  if (!rfid.PICC_ReadCardSerial())
    return;

  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++)
  {
    if (rfid.uid.uidByte[i] < 0x10)
      uid += "0";

    uid += String(rfid.uid.uidByte[i], HEX);
  }

  uid.toUpperCase();

  Serial.println("UID : " + uid);

  String name = "UNKNOWN";
  String status = "DENY";

  // DATABASE KARTU
  if (uid == "0254DB05")
  {
    name = "SAKURA";
    status = "ALLOW";

  }

  sendToSheet(uid, name, status);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  delay(3000);
}

void sendToSheet(String uid, String name, String status)
{

  if (WiFi.status() == WL_CONNECTED)
  {

    HTTPClient http;

    String url = sheetURL;

    url += "?uid=" + uid;
    url += "&name=" + name;
    url += "&status=" + status;

    Serial.println("Sending → " + url);

    http.begin(url);

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int httpCode = http.GET();

    if (httpCode > 0)
    {

      Serial.println("HTTP Response: " + String(httpCode));

      String payload = http.getString();

      Serial.println(payload);
    }
    else
    {

      Serial.println("ERROR SEND");
    }

    http.end();
  }
  else
  {
    Serial.println("WiFi Disconnected");
  }
}
