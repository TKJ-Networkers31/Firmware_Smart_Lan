// ======== LIBRARY ========
#include <Arduino.h>
#include <SPI.h>
#include <DHT.h>
#include <IRremote.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <MFRC522.h>
#include <HTTPClient.h>

// ======== PIN DECLARATION ========
#define RELAY_LED1_2  14
#define RELAY_LED3_4  27
#define SELENOID      17
#define TRIG_PIN      25
#define ECHO_PIN      26
#define KY005         33
#define DHTPIN         4
#define PIN_BUZZER    32
#define LDR           35
#define SS_PIN         5
#define RST_PIN       16

#define DHTTYPE DHT11
#define TEMP_LIMIT 27.0

// ======== ASYNCHRONUS MILLIS TIMERS ========
unsigned long now;
unsigned long lastLDR      = 0;
unsigned long lastDHT      = 0;
unsigned long lastPub      = 0;
unsigned long lastTtl      = 0;
unsigned long lastDistance = 0;
unsigned long doorTimer    = 0;
unsigned long lcdTimer     = 0;

// ======== INTERVAL TIME ========
const unsigned long dhtInterval      = 2000;
const unsigned long ldrInterval      = 500;
const unsigned long pubInterval      = 10000;   // kirim sensor tiap 10 detik
const unsigned long ttlInterval      = 60000;   // heartbeat tiap 1 menit
const unsigned long distanceInterval = 500;
const unsigned long lcdInterval      = 5000;    // refresh tampilan LCD
const unsigned int  doorDuration     = 5000;    // pintu terbuka 5 detik

// ======== IR REMOTE CODE ========
uint32_t AC_ON  = 0x20DF10EF;
uint32_t AC_OFF = 0x20DF906F;

// ======== OBJECT DECLARATION ========
DHT             dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
IRsend          irsend(KY005);
WiFiClient      espClient;
PubSubClient    mqtt(espClient);
MFRC522         rfid(SS_PIN, RST_PIN);

// ======== STATE VARIABLES ========
bool  modeAuto   = true;
bool  locked     = false;
bool  login      = false;
char  user[32]   = "none";
char  idLoginNow[64] = "none";   // UID yang sedang login (normalized)
bool  acStatus   = false;
bool  doorOpen   = false;
float distance   = 0.0;
float suhu       = 0.0;
float kelembapan = 0.0;
int   ldrRead    = 0;
int   thresholdLdr = 50;

// ======== GOOGLE SHEETS ========
const char* sheetURL = "https://script.google.com/macros/s/AKfycbz-zqtx90oHWwHfh-QBQ4vu1HvO1AwoGksLby7PP32SADF9nH6_Z1k-1K2RH75wzUJ64A/exec";

// ======== WIFI & MQTT ========
const char* ssid       = "GEDUNG-S21@TJKT-SMKN2BE";
const char* password   = "tjkt2025";
const char* mqttServer = "172.20.24.11";
const int   mqttPort   = 1883;
const char* clientID   = "esp32_smartlab_1";

// Topik publish
const char* topicSensor    = "lab1/sensor";
const char* topicAccess    = "lab1/access";
const char* topicTTL       = "lab1/timetolive";

// Topik subscribe
const char* topicSubLogin  = "lab1/control/login";
const char* topicSubMode   = "lab1/control/mode";
const char* topicSubLocked = "lab1/control/locked";
const char* topicSubDoor   = "lab1/control/door";
const char* topicSubLamp12 = "lab1/control/lamp1_2";
const char* topicSubLamp34 = "lab1/control/lamp3_4";
const char* topicSubAC     = "lab1/control/ac";

// Buffer JSON
char jsonBuf[1024];

// ======== PROTOTYPES ========
void connectWifi();
void connectMQTT();
void beep(int ms);
void openDoor();
void closeDoor();
float readUltrasonic();
void offAllRelay();
void shutAllDevice();
void sentLogin();
void allPublishStatus();
void sentTTL();
void sendToSheet(const char* uid, const char* name, const char* status);
void lcdShow(const char* line1, const char* line2 = "");

// ======================================================
// NORMALISASI UID
// ESP32 MFRC522 menghasilkan byte per byte, digabung
// dengan format " AB CD EF 01" (uppercase, spasi di depan setiap byte).
// Fungsi ini menghasilkan string tanpa spasi terdepan,
// konsisten uppercase: "AB CD EF 01"
// ======================================================
String buildUID(MFRC522& r) {
    String uid = "";
    for (byte i = 0; i < r.uid.size; i++) {
        if (i > 0) uid += " ";
        if (r.uid.uidByte[i] < 0x10) uid += "0";
        uid += String(r.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    return uid; // contoh: "AB CD EF 01"
}

// ======================================================
// LCD HELPER
// ======================================================
void lcdShow(const char* line1, const char* line2) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1);
    if (line2 && strlen(line2) > 0) {
        lcd.setCursor(0, 1);
        lcd.print(line2);
    }
}

// ======================================================
// WIFI
// ======================================================
void connectWifi() {
    lcdShow("Connecting WiFi", "...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 30) {
        delay(500);
        retry++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        char ipStr[20];
        snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        Serial.print("WiFi OK IP: "); Serial.println(ipStr);
        lcdShow("WiFi Connected", ipStr);
    } else {
        Serial.println("WiFi FAILED");
        lcdShow("WiFi GAGAL", "Cek SSID/Pass");
    }
    delay(1500);
}

// ======================================================
// MQTT
// ======================================================
void connectMQTT() {
    if (mqtt.connected()) return;

    lcdShow("MQTT Connect...", "");
    int retry = 0;
    while (!mqtt.connected() && retry < 5) {
        mqtt.connect(clientID);
        delay(500);
        retry++;
    }

    if (mqtt.connected()) {
        mqtt.subscribe(topicSubLogin);
        mqtt.subscribe(topicSubMode);
        mqtt.subscribe(topicSubLocked);
        mqtt.subscribe(topicSubDoor);
        mqtt.subscribe(topicSubLamp12);
        mqtt.subscribe(topicSubLamp34);
        mqtt.subscribe(topicSubAC);
        Serial.println("MQTT OK");
        lcdShow("MQTT Connected", "Smart Lab Ready");
    } else {
        Serial.println("MQTT FAILED");
        lcdShow("MQTT GAGAL", "Cek Broker");
    }
    delay(1000);
}

// ======================================================
// BUZZER
// ======================================================
void beep(int ms) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(ms);
    digitalWrite(PIN_BUZZER, LOW);
    delay(50);
}

// ======================================================
// PINTU
// ======================================================
void openDoor() {
    if (!doorOpen) {
        digitalWrite(SELENOID, HIGH);
        beep(200);
        doorOpen  = true;
        doorTimer = millis();
        Serial.println("Door OPEN");
    }
}

void closeDoor() {
    if (doorOpen && millis() - doorTimer >= doorDuration) {
        digitalWrite(SELENOID, LOW);
        beep(100);
        doorOpen = false;
        Serial.println("Door CLOSED");
    }
}

// ======================================================
// ULTRASONIC
// ======================================================
float readUltrasonic() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long dur = pulseIn(ECHO_PIN, HIGH, 30000);
    if (dur == 0) return -1;
    return (dur / 2.0) * 0.0343;
}

// ======================================================
// RELAY
// ======================================================
void offAllRelay() {
    digitalWrite(RELAY_LED1_2, LOW);
    digitalWrite(RELAY_LED3_4, LOW);
}

void shutAllDevice() {
    offAllRelay();
    digitalWrite(SELENOID, LOW);
    doorOpen = false;
    Serial.println("All devices shut");
}

// ======================================================
// GOOGLE SHEETS
// ======================================================
void sendToSheet(const char* uid, const char* name, const char* statusStr) {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    char url[512];
    // URL encode spasi → %20
    String uidEnc  = String(uid);  uidEnc.replace(" ", "%20");
    String nameEnc = String(name); nameEnc.replace(" ", "%20");

    snprintf(url, sizeof(url), "%s?uid=%s&name=%s&status=%s",
             sheetURL, uidEnc.c_str(), nameEnc.c_str(), statusStr);

    http.begin(url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(5000);
    int code = http.GET();
    Serial.print("Sheets HTTP: "); Serial.println(code);
    http.end();
}

// ======================================================
// PUBLISH SENSOR
// ======================================================
void allPublishStatus() {
    StaticJsonDocument<1024> doc;

    doc["device"]     = clientID;
    doc["modeAuto"]   = modeAuto;
    doc["locked"]     = locked;
    doc["login"]      = login;
    doc["user"]       = user;
    doc["uid"]        = idLoginNow;

    // Network
    IPAddress ip = WiFi.localIP();
    char ipStr[20];
    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    doc["ip"]   = ipStr;
    doc["rssi"] = WiFi.RSSI();

    // Sensor
    doc["temp"]     = suhu;
    doc["hum"]      = kelembapan;
    doc["light"]    = ldrRead;
    doc["distance"] = distance;

    // Memory
    doc["freeMemory"] = (int)(ESP.getFreeHeap()    / 1024);
    doc["maxAlloc"]   = (int)(ESP.getMaxAllocHeap()/ 1024);

    // Aktuator (baca langsung dari pin, bukan variable)
    doc["door"]    = (digitalRead(SELENOID)    == HIGH);
    doc["lamp1_2"] = (digitalRead(RELAY_LED1_2)== HIGH);
    doc["lamp3_4"] = (digitalRead(RELAY_LED3_4)== HIGH);

    size_t n = serializeJson(doc, jsonBuf, sizeof(jsonBuf));

    if (mqtt.connected()) {
        bool ok = mqtt.publish(topicSensor, jsonBuf, n);
        Serial.print("Publish sensor: "); Serial.println(ok ? "OK" : "FAIL");
    }
}

// ======================================================
// TTL HEARTBEAT
// ======================================================
void sentTTL() {
    StaticJsonDocument<128> doc;
    doc["device"]  = clientID;
    doc["message"] = "alive";
    size_t n = serializeJson(doc, jsonBuf, sizeof(jsonBuf));
    if (mqtt.connected()) mqtt.publish(topicTTL, jsonBuf, n);
}

// ======================================================
// MQTT CALLBACK
// ======================================================
void callback(char* topic, byte* payload, unsigned int length) {
    if (!topic || !payload || length == 0) return;

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        Serial.print("JSON err: "); Serial.println(err.c_str());
        return;
    }

    // ----- lab1/control/login -----
    // Server membalas setelah validasi kartu
    if (strcmp(topic, topicSubLogin) == 0) {
        if (locked) return;  // jika locked, abaikan

        const char* sAccess = doc["statusAccess"] | "denied";
        const char* uName   = doc["user"]         | "none";
        const char* uuid    = doc["uid"]           | "none";

        if (strcmp(sAccess, "success") == 0) {
            login = !login;  // toggle login/logout

            if (login) {
                // LOGIN
                strncpy(user, uName, sizeof(user) - 1);
                user[sizeof(user) - 1] = '\0';
                strncpy(idLoginNow, uuid, sizeof(idLoginNow) - 1);
                idLoginNow[sizeof(idLoginNow) - 1] = '\0';

                openDoor();
                lcdShow("Akses Diterima", user);
                sendToSheet(idLoginNow, uName, "login");
                Serial.print("LOGIN: "); Serial.println(user);
            } else {
                // LOGOUT
                sendToSheet(idLoginNow, uName, "logout");
                offAllRelay();
                openDoor();
                lcdShow("Sampai Jumpa", user);
                Serial.print("LOGOUT: "); Serial.println(user);

                // Reset state
                strncpy(user, "none", sizeof(user));
                strncpy(idLoginNow, "none", sizeof(idLoginNow));
            }

        } else if (strcmp(sAccess, "denied") == 0) {
            lcdShow("Akses Ditolak", "Kartu Tdk Dikenal");
            beep(800);
            sendToSheet(uuid, "tidak dikenal", "denied");
            delay(1500);
            if (!login) {
                lcdShow("Silahkan Login", "Tempel Kartu");
            } else {
                lcdShow("Lab Aktif", user);
            }
        } else if (strcmp(sAccess, "locked") == 0) {
            lcdShow("SISTEM TERKUNCI", "Hubungi Admin");
            beep(300); delay(100); beep(300);
        }
    }

    // ----- lab1/control/mode -----
    if (strcmp(topic, topicSubMode) == 0) {
        bool newMode = doc["mode_auto"] | modeAuto;
        modeAuto = newMode;
        if (!login) shutAllDevice();
        Serial.print("Mode: "); Serial.println(modeAuto ? "AUTO" : "MANUAL");
    }

    // ----- lab1/control/locked -----
    if (strcmp(topic, topicSubLocked) == 0 && !login) {
        bool newLock = doc["locked"] | false;
        locked = newLock;
        if (locked) {
            shutAllDevice();
            lcdShow("LAB TERKUNCI", "Hubungi Admin");
        } else {
            lcdShow("Lab UNLOCKED", "Silahkan Login");
        }
        Serial.print("Locked: "); Serial.println(locked);
    }

    // ----- lab1/control/door (hanya mode manual) -----
    if (strcmp(topic, topicSubDoor) == 0 && !locked && !modeAuto) {
        bool doorVal = doc["door"] | false;
        if (doorVal) openDoor();
        else {
            digitalWrite(SELENOID, LOW);
            doorOpen = false;
        }
    }

    // ----- lab1/control/lamp1_2 (hanya mode manual) -----
    if (strcmp(topic, topicSubLamp12) == 0 && !locked && !modeAuto) {
        bool val = doc["lampu1_2"] | false;
        digitalWrite(RELAY_LED1_2, val ? HIGH : LOW);
        Serial.print("Lamp1_2: "); Serial.println(val);
    }

    // ----- lab1/control/lamp3_4 (hanya mode manual) -----
    if (strcmp(topic, topicSubLamp34) == 0 && !locked && !modeAuto) {
        bool val = doc["lampu3_4"] | false;
        digitalWrite(RELAY_LED3_4, val ? HIGH : LOW);
        Serial.print("Lamp3_4: "); Serial.println(val);
    }

    // ----- lab1/control/ac -----
    if (strcmp(topic, topicSubAC) == 0) {
        // Implementasi kontrol AC via IR
        bool acOn = doc["ac"] | false;
        if (acOn && !acStatus) {
            IrSender.sendNEC(AC_ON, 32);
            acStatus = true;
        } else if (!acOn && acStatus) {
            IrSender.sendNEC(AC_OFF, 32);
            acStatus = false;
        }
    }
}

// ======================================================
// RFID: KIRIM REQUEST LOGIN/LOGOUT
// ======================================================
void sentLogin() {
    if (!rfid.PICC_IsNewCardPresent()) return;
    if (!rfid.PICC_ReadCardSerial())   return;

    String uid = buildUID(rfid);
    Serial.print("RFID UID: "); Serial.println(uid);

    StaticJsonDocument<256> doc;
    doc["uid"]    = uid;
    doc["device"] = clientID;

    // Tentukan status berdasarkan kondisi login saat ini
    // Jika sudah login dan UID cocok → logout; jika belum login → login
    if (login && strcmp(idLoginNow, uid.c_str()) == 0) {
        doc["status"] = "logout";
        Serial.println("Request LOGOUT");
    } else if (!login) {
        doc["status"] = "login";
        Serial.println("Request LOGIN");
    } else {
        // Ada orang lain yang tap kartu saat sudah login → denied lokal
        Serial.println("Tap oleh orang lain, diabaikan");
        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();
        beep(200);
        return;
    }

    size_t n = serializeJson(doc, jsonBuf, sizeof(jsonBuf));
    if (mqtt.connected()) {
        mqtt.publish(topicAccess, jsonBuf, n);
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    delay(500); // debounce kartu
}

// ======================================================
// SETUP
// ======================================================
void setup() {
    Serial.begin(9600);
    delay(1000);

    // Init peripheral
    SPI.begin();
    rfid.PCD_Init();
    delay(100);
    dht.begin();
    delay(100);
    lcd.init();
    lcd.backlight();
    IrSender.begin(KY005);

    // Pin modes
    pinMode(TRIG_PIN,    OUTPUT);
    pinMode(ECHO_PIN,    INPUT);
    pinMode(LDR,         INPUT);
    pinMode(PIN_BUZZER,  OUTPUT);
    pinMode(SELENOID,    OUTPUT);
    pinMode(RELAY_LED1_2,OUTPUT);
    pinMode(RELAY_LED3_4,OUTPUT);

    offAllRelay();
    digitalWrite(SELENOID, LOW);

    lcdShow("Smart Lab IOT", "SMKN 2 Baleendah");
    delay(2000);

    // Koneksi
    connectWifi();
    mqtt.setServer(mqttServer, mqttPort);
    mqtt.setBufferSize(1024);
    mqtt.setCallback(callback);
    connectMQTT();

    lcdShow("Sistem Siap", "Tempel Kartu");
    delay(1000);

    Serial.println("=== ESP32 Smart Lab Ready ===");
}

// ======================================================
// LOOP
// ======================================================
void loop() {
    now = millis();

    // Reconnect jika putus
    if (WiFi.status() != WL_CONNECTED) connectWifi();
    if (!mqtt.connected())             connectMQTT();
    mqtt.loop();

    // Baca DHT tiap 2 detik
    if (now - lastDHT >= dhtInterval) {
        lastDHT = now;
        float t = dht.readTemperature();
        float h = dht.readHumidity();
        if (!isnan(t)) suhu       = t;
        if (!isnan(h)) kelembapan = h;
    }

    // Baca LDR
    ldrRead = analogRead(LDR);

    // Baca jarak
    if (now - lastDistance >= distanceInterval) {
        lastDistance = now;
        distance = readUltrasonic();
    }

    // Publish sensor tiap 10 detik
    if (now - lastPub >= pubInterval) {
        lastPub = now;
        allPublishStatus();
    }

    // TTL heartbeat tiap 1 menit
    if (now - lastTtl >= ttlInterval) {
        lastTtl = now;
        sentTTL();
    }

    // ======== LOGIKA MODE AUTO ========
    if (modeAuto) {
        if (locked) {
            // Sistem terkunci
            if (now - lcdTimer >= lcdInterval) {
                lcdTimer = now;
                lcdShow("LAB TERKUNCI", "Hubungi Admin");
            }
        } else {
            // Tidak terkunci, proses RFID
            sentLogin();

            if (login) {
                // === SUDAH LOGIN ===
                // Kontrol lampu otomatis berdasarkan cahaya
                if (ldrRead <= thresholdLdr) {
                    digitalWrite(RELAY_LED1_2, HIGH);
                    digitalWrite(RELAY_LED3_4, HIGH);
                } else {
                    digitalWrite(RELAY_LED1_2, LOW);
                    digitalWrite(RELAY_LED3_4, LOW);
                }

                // Buka pintu otomatis jika ada objek dekat (sensor)
                if (distance > 0 && distance <= 10.0) {
                    openDoor();
                }

                // Kontrol AC otomatis
                if (suhu >= TEMP_LIMIT && !acStatus) {
                    IrSender.sendNEC(AC_ON, 32);
                    acStatus = true;
                    Serial.println("AC ON (auto)");
                } else if (suhu < TEMP_LIMIT - 1 && acStatus) {
                    IrSender.sendNEC(AC_OFF, 32);
                    acStatus = false;
                    Serial.println("AC OFF (auto)");
                }

                // Tampilan LCD saat login
                if (now - lcdTimer >= lcdInterval) {
                    lcdTimer = now;
                    char line2[17];
                    snprintf(line2, sizeof(line2), "T:%.1f H:%.0f%%", suhu, kelembapan);
                    lcdShow(user, line2);
                }

            } else {
                // === BELUM LOGIN ===
                // Matikan semua perangkat
                if (digitalRead(RELAY_LED1_2) || digitalRead(RELAY_LED3_4)) {
                    offAllRelay();
                }
                if (acStatus) {
                    IrSender.sendNEC(AC_OFF, 32);
                    acStatus = false;
                }

                if (now - lcdTimer >= lcdInterval) {
                    lcdTimer = now;
                    lcdShow("Silahkan Login", "Tempel Kartu");
                }
            }
        }
    }
    // ======== MODE MANUAL ========
    // Di mode manual, kontrol dari server via MQTT callback

    closeDoor();
}