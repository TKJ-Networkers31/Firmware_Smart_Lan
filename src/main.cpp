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


// ======== PIN DECLARATION ========
#define RELAY_LED2_3  14
#define RELAY_LED1_4  27
#define SELENOID      17
#define TRIG_PIN      25
#define ECHO_PIN      26
#define KY005         33
#define DHTPIN         4
#define PIN_BUZZER    32
#define LDR           35
#define SS_PIN        5
#define RST_PIN       16

#define DHTTYPE DHT11
#define TEMP_LIMIT 27.0

unsigned long now;

// ======== ASYNCHRONUS TIME FOR MILIS ========
unsigned long lastLDR = 0;
unsigned long lastDHT = 0;
unsigned long lastPub = 0;
unsigned long lastTtl = 0;
unsigned long lastLcd = 0;
unsigned long doorTimer = 0;

// ======== INTERVAL TIME FOR MILIS ========
const unsigned long dhtInterval = 2000;
const unsigned long ldrInterval = 500;
const unsigned long pubInterval = 5000;
const unsigned long ttlInterval = 6000000;
const unsigned long lcdInterval = 3000;
const unsigned int doorDuration = 5000;


// ======== UNIQCODE INFRARED ========
uint32_t AC_ON = 0x20DF10EF;   // contoh NEC
uint32_t AC_OFF = 0x20DF906F; //beta code

// ======== OBJECT DELARATION ========
DHT dht(DHTPIN, DHTTYPE); //object sensor suhu
LiquidCrystal_I2C lcd(0x27,16, 2); //object lcd i2c
IRsend irsend(KY005); //object sensor infrared
WiFiClient espClient; //object untuk unit wifi
PubSubClient mqtt(espClient); //object untuk mqtt protocol
MFRC522 rfid(SS_PIN, RST_PIN); //object sensor RFID

// ======== VARIABLE ========

bool modeAuto = true; //jika false maka akan masuk mode manual kontrol via web 
bool locked = false; //mode lock maka ruang tidak bisa di akses
bool login = false; //keterangan kondisi apakah ada yang login atau sebaliknya
char user[16] = "none"; //ada 2 kemungkinan "none" atau "name user"
char idLoginNow[64] = "none";
char statusAcces[16] = "denied"; //by default

bool acStatus = false; // status ac ruangan
int relay[] = {27,14}; //array relay pin
float suhuTrigger = 25.0; //nilai patokan suhu
//int hasil = 0; //pb status
int ldrRead = 0; //hasil banya sensor ldr/cahaya
int thresholdLdr = 50; //nilai patikan cahaya pada sensor ldr
bool doorOpen = false; //status pintu buka atau tertutup
float distance = 0.0; //hasil baca sensor ultrasonic
float suhu = 0.0; //hasil baca dht untuk suhu
float kelembapan = 0.0;  //hasil baca sensor dht untuk kelembapan

// ======== VARIABEL FOR WIFI ========
const char* ssid = "redmi9c";
const char* password = "11117994";
const char* mqttServer = "192.168.43.87";
const int   mqttPort   = 1883;
const char* clientID   = "esp32_smartlab1";
const char* topicPub[]   = {
  "lab1/sensor",
  "lab1/access",
  "lab1/timetolive"
};
const char* topicSub[] = {
  "lab1/control/login",
  "lab1/control/mode",
  "lab1/control/lock",
  "lab1/control/door",
  "lab1/control/lamp",
  "lab1/control/ac",
};

const char* codeAc[] = {
  //in progres
};

// ---------- BUFFERS ----------
char jsonBuf[1024];

// ======== FUNCTION DECLARATION ========

//print lcd 
void lcdi2c_1(const char* one){
  lcd.setCursor(0,0);
  lcd.print(one);

}
void lcdi2c_2(const char* one, const char* two){
  lcd.setCursor(0,0);
  lcd.print(one);
  lcd.setCursor(0,1);
  lcd.print(two);
}

// wifi function
void connectWifi(){
  lcd.clear();
  lcdi2c_1("WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(300);
    lcd.print(".");
    retry++;
  }

  char line1[32];
  char line2[32];

  if (WiFi.status() == WL_CONNECTED) {
    IPAddress ip = WiFi.localIP();
    snprintf(line1, sizeof(line1), "IP:%d.%d.%d.%d",
             ip[0], ip[1], ip[2], ip[3]);
    snprintf(line2, sizeof(line2), "SSID:%s", WiFi.SSID().c_str());
  } else {
    snprintf(line1, sizeof(line1), "WiFi FAILED");
    snprintf(line2, sizeof(line2), "Check SSID");
  }

  lcd.clear();
  lcdi2c_2(line2, line1);
  Serial.println(WiFi.SSID());
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.RSSI());
}

//connect to mqtt broker
void connectMQTT() {
  if (mqtt.connected()) return;
  int retry = 0;
  Serial.println("melakukan koneksi");
  while (!mqtt.connected() && retry < 5) {
    Serial.print(".");
    mqtt.connect(clientID);
    delay(500);
    retry++;
  }
  if(mqtt.connected()){
    for(int i = 0; i<=6 ;i++){
      mqtt.subscribe(topicSub[i]);
      Serial.println("subscribe ");
      Serial.println(topicSub[i]);

    }
    Serial.println("connected to broker");
    lcdi2c_2("connected to","broker");
  }
}

// buzzer/alarm function
void beep(int ms) {
  delay(50);
  digitalWrite(PIN_BUZZER, HIGH);
  delay(ms);
  digitalWrite(PIN_BUZZER, LOW);
}

//open door function
void openDoor(){
  if(!doorOpen){
    digitalWrite(SELENOID, HIGH);
    beep(200);
    doorOpen = true;
    doorTimer = millis();
    Serial.println("Door OPEN");
  }
}

//close door function
void closeDoor(){
  if(doorOpen && millis() - doorTimer >= doorDuration){
    digitalWrite(SELENOID, LOW);
    beep(200);
    doorOpen = false;
    Serial.println("Door CLOSED");
  }
}

//membaca jarak
float readUltrasonic() {
  long durasi;
  float jarak;

  // 1. Kirim pulsa
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // 2. Baca waktu pantulan
  durasi = pulseIn(ECHO_PIN, HIGH, 30000);

  // 3. Cek gagal atau tidak
  if (durasi == 0) {
    return -1; // tanda error
  }

  // 4. Hitung jarak
  jarak = (durasi / 2.0) * 0.0343;

  return jarak;
}

void offAllRelay(){
  int pin[] = {14,27,16};
  for(int i = 0; i<=2; i++){
    digitalWrite(pin[i], LOW);
  }
}

void acControl(char command){
  //kontrol ac
}

void shutAllDevice(){
  offAllRelay();
  digitalWrite(SELENOID,LOW);
  //sent signal
}

//kirim pesan eror (JSON)
void sentErrorMassage(){
  //comming soon
}

void sentTTL(const char* client){
  StaticJsonDocument<256> doc;

  doc["device"] = client;
  doc["massage"] = "hello";
  size_t n = serializeJson(doc,jsonBuf, sizeof(jsonBuf));
  mqtt.publish(topicPub[2], jsonBuf, n);
}

//publish all data to broker
void allPublishStatus(float temprature, float humadity, int light, const char* client , bool login, char* user, bool modeAuto, bool locked) {
  StaticJsonDocument<1024> doc;
  
  doc["device"] = client;
  doc["modeAuto"] = modeAuto;
  doc["locked"] = locked;
  doc["login"] = login;
  doc["user"] = user;
  doc["ip"] = WiFi.localIP();
  doc["rssi"] = WiFi.RSSI();

  doc["temp"] = temprature;
  doc["hum"] = humadity;
  doc["light"] = light;
  doc["distance"] = distance;

  doc["freeMemory"] = ESP.getFreeHeap()/1024;
  doc["maxAlloc"] = ESP.getMaxAllocHeap()/1024;

  doc["door"] = digitalRead(SELENOID) ? "OPEN" : "CLOSED";
  doc["lamp1_4"] = digitalRead(RELAY_LED1_4) ? "ON" : "OFF";
  doc["lamp2_3"] = digitalRead(RELAY_LED2_3) ? "ON" : "OFF";

  size_t n = serializeJson(doc, jsonBuf, sizeof(jsonBuf));
  Serial.print("JSON size: ");
  Serial.println(n);

  if (mqtt.connected()) {
    Serial.println("mqtt connected");
    if (mqtt.publish(topicPub[0], jsonBuf, n)) {
      Serial.println("Publish Berhasil");
    } else {
      Serial.println("Publish GAGAL");
    }
  }else{
    Serial.println("wqtt disconnected");
  }
}

//mqtt callback
// void callback(char* topic, byte* payload, unsigned int length) {
//   StaticJsonDocument<256> doc;
//   DeserializationError error = deserializeJson(doc, payload, length);
//   if (error) return;
//   //eksekusi respon login
//   if (strcmp(topic, topicSub[0]) == 0 && !locked) {
//     const char* sAccess = doc["statusAccess"] | "denied"; // Default jika null
//     const char* uName = doc["user"] | "none";
//     if (strcmp(sAccess, "success") == 0) {
//       login = !login;
//       if (login) {
//         strncpy(user, uName, sizeof(user) - 1);
//         user[sizeof(user) - 1] = '\0';
//         openDoor();
//         lcd.clear();
//         lcd.setCursor(0,0);
//         lcd.print(sAccess);
//         lcd.setCursor(0,1);
//         lcd.print("Hi ");
//         lcd.print(user);
//         Serial.print("Login status: ");
//         Serial.println(login);
//       } else {
//         strcpy(user, "none");
//         openDoor();
//         lcd.clear();
//         lcd.setCursor(0,0);
//         lcd.print(sAccess);
//         lcd.setCursor(0,1);
//         lcd.print("bye ");
//         lcd.print(user);
//         Serial.print("Login status: ");
//         Serial.println(login);
//       }
//     } else {
//       lcd.clear();
//       lcdi2c_2("Akses Ditolak", "Kartu Salah");
//       beep(1000);
//     }
//   }
//   //auto mode
//   if(strcmp(topic, topicSub[1]) == 0){
//     if(strcmp(doc["mode"],"auto")==0){
//       modeAuto = true;
//     }else if(strcmp(doc["mode"],"manual")==0){
//       modeAuto = false;
//     }else{
//       //sentErrorMassage(); #coming soon
//     }
//   }
//   //lock mode
//   if(strcmp(topic, topicSub[2]) == 0 && login == false){
//     if(strcmp(doc["lock"],"lock")==0){
//       lcdi2c_1("lock");
//       locked = true;
//       shutAllDevice();

//     }else if(strcmp(doc["lock"],"unlock")==0){
//       locked = false;
//     }else{
//       //sentErrorMassage(); #coming soon
//     }
//   }
//   //door control
//   if (strcmp(topic, topicSub[3]) == 0 && !locked && modeAuto == false) {
//     (strcmp(doc["door"], "HIGH") == 0)? digitalWrite(SELENOID,HIGH):digitalWrite(SELENOID,LOW);
//   }
//   //lamp cotroll
//   if (strcmp(topic, topicSub[4]) == 0 && !locked && modeAuto == false) {
//     (strcmp(doc["lamp2"], "HIGH") == 0)? digitalWrite(RELAY_LED2_3,HIGH):digitalWrite(RELAY_LED2_3,LOW);
//     (strcmp(doc["lamp4"], "HIGH") == 0)? digitalWrite(RELAY_LED1_4,HIGH):digitalWrite(RELAY_LED1_4,LOW);
//   }
//   //ac control
//   if(strcmp(topic, topicSub[5]) == 0){
//     //ac code in progres
//   }
// }

void callback(char* topic, byte* payload, unsigned int length) {
  // 1. Validasi awal: Pastikan topic dan payload tidak NULL
  if (topic == NULL || payload == NULL || length == 0) {
    return; 
  }

  // 2. Deserialisasi JSON dengan proteksi
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print(F("JSON Parsing Error: "));
    Serial.println(error.c_str());
    return;
  }

  // 3. Eksekusi Respon Login (Topic 0)
  // Selalu cek topicSub[index] != NULL sebelum strcmp
  if (topicSub[0] != NULL && strcmp(topic, topicSub[0]) == 0 && !locked) {
    // Gunakan operator '|' untuk memberikan nilai default jika key tidak ada
    const char* sAccess = doc["statusAccess"] | "denied";
    const char* uName = doc["user"] | "none";

    if (strcmp(sAccess, "success") == 0) {
      // login = true; // Lebih aman set true secara eksplisit daripada !login
      // strncpy(user, uName, sizeof(user) - 1);
      // user[sizeof(user) - 1] = '\0';
      // Salin nama user dengan aman
      login = !login;
      if (login) {
        strncpy(user, uName, sizeof(user) - 1);
        user[sizeof(user) - 1] = '\0';
        openDoor();
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(sAccess);
        lcd.setCursor(0,1);
        lcd.print("Hi ");
        lcd.print(user);
        Serial.print("Login status: ");
        Serial.println(login);
      } else {
        strcpy(user, "none");
        openDoor();
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(sAccess);
        lcd.setCursor(0,1);
        lcd.print("bye ");
        lcd.print(user);
        Serial.print("Login status: ");
        Serial.println(login);
      }
    } else {
      lcd.clear();
      lcdi2c_2("Akses Ditolak", "Kartu Salah");
      beep(1000);
    }
  }

  // 4. Auto Mode (Topic 1)
  if (topicSub[1] != NULL && strcmp(topic, topicSub[1]) == 0) {
    const char* mode = doc["mode"] | ""; // Default string kosong agar strcmp tidak crash
    if (strcmp(mode, "auto") == 0) {
      modeAuto = true;
    } else if (strcmp(mode, "manual") == 0) {
      modeAuto = false;
    }
  }

  // 5. Lock Mode (Topic 2)
  if (topicSub[2] != NULL && strcmp(topic, topicSub[2]) == 0 && !login) {
    const char* lockCmd = doc["lock"] | "";
    if (strcmp(lockCmd, "lock") == 0) {
      lcdi2c_1("lock");
      locked = true;
      shutAllDevice();
    } else if (strcmp(lockCmd, "unlock") == 0) {
      locked = false;
    }
  }

  // 6. Door Control (Topic 3)
  if (topicSub[3] != NULL && strcmp(topic, topicSub[3]) == 0 && !locked && !modeAuto) {
    const char* doorVal = doc["door"] | "LOW";
    digitalWrite(SELENOID, (strcmp(doorVal, "HIGH") == 0) ? HIGH : LOW);
  }

  // 7. Lamp Control (Topic 4)
  if (topicSub[4] != NULL && strcmp(topic, topicSub[4]) == 0 && !locked && !modeAuto) {
    const char* l2 = doc["lamp2"] | "LOW";
    const char* l4 = doc["lamp4"] | "LOW";
    digitalWrite(RELAY_LED2_3, (strcmp(l2, "HIGH") == 0) ? HIGH : LOW);
    digitalWrite(RELAY_LED1_4, (strcmp(l4, "HIGH") == 0) ? HIGH : LOW);
  }

  // 8. AC Control (Topic 5)
  if (topicSub[5] != NULL && strcmp(topic, topicSub[5]) == 0) {
    // Implementasi AC di sini
  }
}

//mengirim request login
void sentLogin() {
  if (!login || (strcmp(user,"none")==0 && strcmp(idLoginNow,"none")==0)){
    // Cek apakah ada kartu baru di dekat reader
    if ( ! rfid.PICC_IsNewCardPresent()) {
      return;
    }
    // // Pilih salah satu kartu
    if ( ! rfid.PICC_ReadCardSerial()) {
      return;
    }
    Serial.print("UID Tag :");
    String id= "";
    for (byte i = 0; i < rfid.uid.size; i++) {
       Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
       Serial.print(rfid.uid.uidByte[i], HEX);
       id.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
       id.concat(String(rfid.uid.uidByte[i], HEX));
    }
    Serial.println();
    id.toUpperCase();
    strcpy(idLoginNow, id.c_str());
    StaticJsonDocument<256> doc;
    doc["id"] = id;
    lcd.clear();

    size_t n = serializeJson(doc, jsonBuf, sizeof(jsonBuf));
    mqtt.publish(topicPub[1], jsonBuf, n);
    rfid.PICC_HaltA();
  }else{
    // Cek apakah ada kartu baru di dekat reader
    if ( ! rfid.PICC_IsNewCardPresent()) {
      return;
    }
    // Pilih salah satu kartu
    if ( ! rfid.PICC_ReadCardSerial()) {
      return;
    }
    Serial.print("UID Tag :");
    String id= "";
    for (byte i = 0; i < rfid.uid.size; i++) {
       Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
       Serial.print(rfid.uid.uidByte[i], HEX);
       id.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
       id.concat(String(rfid.uid.uidByte[i], HEX));
    }
    Serial.println();
    id.toUpperCase();
    if (strcmp(idLoginNow,id.c_str())==0){
      StaticJsonDocument<256> doc;
      doc["id"] = id;
      lcd.clear();
      size_t n = serializeJson(doc, jsonBuf, sizeof(jsonBuf));
      mqtt.publish(topicPub[1], jsonBuf, n);
      rfid.PICC_HaltA();
    }else{
      lcdi2c_2("kartu tidak terdaftar", "tunggu....");
    }  
  }
}

//main logic
void setup() {
  delay(2000);
  //starting system
  Serial.begin(9600);
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());
  delay(1000);
  SPI.begin();
  rfid.PCD_Init();
  delay(500);
  dht.begin();
  delay(500);
  lcd.init();
  lcd.backlight();
  delay(500);
  IrSender.begin(KY005);
  Serial.println("all library active");
  
  delay(2000);
  connectWifi();
  mqtt.setServer(mqttServer, mqttPort);
  mqtt.setBufferSize(1024);

  delay(1000);
  connectMQTT();
  mqtt.setCallback(callback);
  lcdi2c_2("esp 32 ready sir","ready for work");
  delay(3000);
  lcd.clear();
  
  //deklarasi input output
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LDR, INPUT); 
  pinMode(PIN_BUZZER, OUTPUT); 
  pinMode(SELENOID, OUTPUT); 
  // digitalWrite(SELENOID, LOW); 
  for(int i = 0; i <= 1; i++){
    pinMode(relay[i], OUTPUT);
    // digitalWrite(relay[i], LOW); 
  }
  offAllRelay();
  Serial.println("ESP32 IoT Ready");
  lcd.clear();
  lcdi2c_1("iot is ready");
  delay(2000);
}

void loop() {
  now = millis(); //memlai perhitungan cpu untuk asynchronus

  ldrRead = analogRead(LDR);
  if (!mqtt.connected()) {
    connectMQTT();
  }

  mqtt.loop();

  if (now - lastDHT >= dhtInterval) {
    lastDHT = now;
    suhu = dht.readTemperature();
    kelembapan = dht.readHumidity();
    if (isnan(suhu) || isnan(kelembapan)) {
      Serial.println("Gagal baca DHT!");
    }
    char line[32];
    snprintf(line, sizeof(line), "T:%.1fC H:%.0f%%", suhu, kelembapan);
    Serial.println(line);
  }

  if (millis() - lastPub >= pubInterval) {
    lastPub = millis();
    allPublishStatus(suhu,kelembapan,ldrRead,clientID,login, user, modeAuto, locked);
    Serial.println("publish");
  }

  if (millis() - lastTtl >= ttlInterval) {
    lastTtl = millis();
    sentTTL(clientID);
    Serial.println("Update Time To live");
  }
  
  if(modeAuto){
    if(!locked){
      sentLogin();
      if(login){
        lcdi2c_2("tempelkan kartu","untuk logout");
        if(ldrRead <= thresholdLdr){
          for(int i = 0; i<=1; i++){
            digitalWrite(relay[i],HIGH);
          }
          // delay(100);
        }else{
          for(int i = 0; i<=1; i++){
            digitalWrite(relay[i],LOW);
          }
          // delay(100);
        }
        distance = readUltrasonic();
        Serial.print("cm : ");
        Serial.println(distance);
        if (distance <= 10.0){
          openDoor();
        }
      }else{
        lcdi2c_2("tempelkan kartu","untuk login");
      }
    }else{
      // lcdi2c_1("locked");
    }
    closeDoor();
    if (suhu >= TEMP_LIMIT && !acStatus && login == true) {
      // Serial.println("AC ON");
      IrSender.sendNEC(AC_ON, 32);
      acStatus = true;
    }
    if ((suhu < TEMP_LIMIT -1 && acStatus && login) || !login) {
      // Serial.println("AC OFF");
      IrSender.sendNEC(AC_OFF, 32);
      acStatus = false;
    }
    // else{
    //   for(int i = 0; i<=2; i++){
    //     digitalWrite(relay[i],LOW);
    //   }
    // }
  }
  // delay(100);
}
