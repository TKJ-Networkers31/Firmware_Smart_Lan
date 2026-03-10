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
//#define PB1         34
#define RELAY_LED1  27
#define RELAY_LED2  13
#define RELAY_LED3  14
#define RELAY_LED4  27
#define SELENOID    17
#define TRIG_PIN    25
#define ECHO_PIN    26
#define KY005       33
#define DHTPIN       4
#define PIN_BUZZER  32
#define LDR         35
#define SS_PIN      5
#define RST_PIN     16

#define DHTTYPE DHT11
#define TEMP_LIMIT 27.0

// ======== ASYNCHRONUS TIME FOR MILIS ========
unsigned long lastLDR = 0;
unsigned long lastDHT = 0;
unsigned long lastPub = 0;
unsigned long doorTimer = 0;

// ======== INTERVAL TIME FOR MILIS ========
const unsigned long dhtInterval = 2000;
const unsigned long ldrInterval = 500;
const unsigned long pubInterval = 2000;
const unsigned int doorDuration = 5000;


// ======== UNIQCODE INFRARED ========
uint32_t AC_ON = 0x20DF10EF;   // contoh NEC
uint32_t AC_OFF = 0x20DF906F;

// ======== OBJECT DELARATION ========
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27,16, 2);
IRsend irsend(KY005);
WiFiClient espClient;
PubSubClient mqtt(espClient);
MFRC522 rfid(SS_PIN, RST_PIN);

// ======== VARIABLE ========
bool modeAuto = true; //jika false maka akan masuk mode manual kontrol via web
bool login = false; 
char user[16] = "none"; //ada 2 kemungkinan "none" atau "name user"
char statusAcces[16] = "denied"; //by default
bool acStatus = false;
int relay[] = {27,13,14,15}; //relay pin
float suhuTrigger = 25.0;
//int hasil = 0; //pb status
int ldrRead = 0;
int thresholdLdr = 50;
bool doorOpen = false;
float distance = 0.0;
float suhu = 0.0;
float kelembapan = 0.0;

// ======== VARIABEL FOR WIFI ========
const char* ssid = "redmi9c";
const char* password = "11117994";
const char* mqttServer = "192.168.43.87";
const int   mqttPort   = 1883;
const char* clientID   = "esp32_smartlab1";
const char* topicPub[]   = {
  "lab1/sensor",
  "lab1/access"
};
const char* topicSub[] = {
  "lab1/control/login",
  "lab1/control/door",
  "lab1/control/lamp",
  "lab1/control/mode",
  //"lab1/control/lamp1",
  //"lab1/control/lamp3",
  //"lab1/control/lamp4",
  //"lab1/control/door",
  "lab1/control/ac"
};

const char* codeAc[] = {
  //in progres
};

// ---------- BUFFERS ----------
char uidBuf[32];
char jsonBuf[2048];
char mqttBuf[192];

// ======== FUNCTION DECLARATION ========

//print lcd 
void lcdi2c(const char* one, const char* two){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(one);
  lcd.setCursor(0,1);
  lcd.print(two);
}

// wifi function
void connectWifi(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WiFi...");

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

  lcdi2c(line2, line1);
  Serial.println(WiFi.SSID());
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.RSSI());
}

//connect to mqtt broker
void connectMQTT() {
  if (mqtt.connected()) return;
  while (!mqtt.connected()) {
    mqtt.connect(clientID);
    delay(300);
  }
  for(int i = 0; i<=4 ;i++){
    mqtt.subscribe(topicSub[i]);
  }
  lcd.clear();
  lcdi2c("connected to","broker");
}

//publish to broker
void allPublishStatus(float temprature, float humadity, int light, const char* client , bool login, char* user) {
  StaticJsonDocument<2048> doc;
  
  doc["device"] = client;
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
  doc["lamp1"] = digitalRead(RELAY_LED1) ? "ON" : "OFF";
  doc["lamp2"] = digitalRead(RELAY_LED2) ? "ON" : "OFF";
  doc["lamp3"] = digitalRead(RELAY_LED3) ? "ON" : "OFF";
  doc["lamp4"] = digitalRead(RELAY_LED4) ? "ON" : "OFF";

  size_t n = serializeJson(doc, jsonBuf, sizeof(jsonBuf));
  // Serial.println(jsonBuf);
  mqtt.publish(topicPub[0], jsonBuf, n);
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

//mqtt callback
void callback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) return;
  if (strcmp(topic, "lab1/control/login") == 0) {
    strcpy(user,doc["statusAccess"]);
    if(strcmp(statusAcces, "success")==0){
      login = !login; //mengubah status login
      Serial.println(login ? "User login" : "User logout"); //untuk debug
      Serial.print("status ");
      Serial.print(login);
      Serial.println(""); //hingga sini debug
      openDoor();
      if (login) {
        strcpy(user,doc["user"]);
      } else {
        strcpy(user,"none");
      } //atur nama user
      char buffer[50]; //wadah untuk serial
      snprintf(buffer, sizeof(buffer), "halo %s", user);
      lcdi2c(statusAcces,buffer);
      delay(400);   
    }else{
      lcdi2c(statusAcces,"kartu tidak ada");
      beep(1000);
    }
  }
  if (strcmp(topic, "lab1/control/door") == 0) {
    digitalWrite(SELENOID,LOW);
  }
  if (strcmp(topic, "lab1/control/lamp") == 0) {
    (strcmp(doc["lamp1"], "HIGH") == 0)? digitalWrite(RELAY_LED1,HIGH):digitalWrite(RELAY_LED1,LOW);
    (strcmp(doc["lamp2"], "HIGH") == 0)? digitalWrite(RELAY_LED1,HIGH):digitalWrite(RELAY_LED1,LOW);
    (strcmp(doc["lamp3"], "HIGH") == 0)? digitalWrite(RELAY_LED1,HIGH):digitalWrite(RELAY_LED1,LOW);
    (strcmp(doc["lamp4"], "HIGH") == 0)? digitalWrite(RELAY_LED1,HIGH):digitalWrite(RELAY_LED1,LOW);
  }
  if(strcmp(topic, "lab1/control/mode") == 0){
    modeAuto = !modeAuto;
  }
  if(strcmp(topic, "lab1/control/ac") == 0){
    //in progres
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

//capture psram
const char* capturePsram(){
  static char line2[32];

  if (!psramFound()){
    snprintf(line2, sizeof(line2), "psram:notfound");
  } else {
    snprintf(line2, sizeof(line2), "psram:%lu", ESP.getFreePsram()/1024);
  }
  return line2;
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

void sentLogin() {
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
  StaticJsonDocument<1024> doc;
  doc["id"] = id;
  size_t n = serializeJson(doc, jsonBuf, sizeof(jsonBuf));
  mqtt.publish(topicPub[1], jsonBuf, n);
  rfid.PICC_HaltA();
}

void setup() {
  //starting system
  Serial.begin(9600);
  SPI.begin();
  dht.begin();
  lcd.init();
  lcd.backlight();
  IrSender.begin(KY005);
  connectWifi();
  mqtt.setServer(mqttServer, mqttPort);
  connectMQTT();
  // mqtt.setCallback(mqttCallback);
  rfid.PCD_Init();
  delay(3000);
  lcdi2c("esp 32 ready sir", (psramFound())? "psram found":"psram notfound");
  delay(3000);

  //deklarasi input output
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  //pinMode(PB1, INPUT); 
  pinMode(LDR, INPUT); 
  pinMode(PIN_BUZZER, OUTPUT); 
  for(int i = 0; i <= 3; i++){
    pinMode(relay[i], OUTPUT);
    digitalWrite(relay[i], LOW); 
  }
  Serial.println("ESP32 IoT Ready");
  delay(2000);
}

void loop() {
  unsigned long now = millis(); //memlai perhitungan cpu untuk asynchronus
  //baca hasil
  //hasil = digitalRead(PB1);
  ldrRead = analogRead(LDR);
  mqtt.loop();
  if (now - lastDHT >= dhtInterval) {
    lastDHT = now;
    suhu = dht.readTemperature();
    kelembapan = dht.readHumidity();
    if (isnan(suhu) || isnan(kelembapan)) {
      Serial.println("Gagal baca DHT!");
      return;
    }
    // char line[32];
    // snprintf(line, sizeof(line), "T:%.1fC H:%.0f%%", suhu, kelembapan);
    // Serial.println(line);
  }

  if(modeAuto){
    closeDoor();
    if (suhu >= TEMP_LIMIT && !acStatus && login == true) {
      // Serial.println("AC ON");
      IrSender.sendNEC(AC_ON, 32);
      acStatus = true;
    }
    if (suhu < TEMP_LIMIT - 1 && acStatus && login == true || login == false) {
      // Serial.println("AC OFF");
      IrSender.sendNEC(AC_OFF, 32);
      acStatus = false;
    }

    if(login){
      if(ldrRead <= thresholdLdr){
        for(int i = 0; i<=2; i++){
          digitalWrite(relay[i],HIGH);
        }
        delay(100);
      }else{
        for(int i = 0; i<=2; i++){
          digitalWrite(relay[i],LOW);
        }
        delay(100);
      }
      distance = readUltrasonic();
      // Serial.print("cm : ");
      // Serial.println(distance);
      if (distance <= 10.0){
        openDoor();
      }
      
    }else{
      for(int i = 0; i<=2; i++){
        digitalWrite(relay[i],LOW);
      }
    }
  }

  if (now - lastLDR >= ldrInterval) {
    lastLDR = now;

    uint32_t freeHeap = ESP.getFreeHeap()/1024;
    uint32_t maxAlloc = ESP.getMaxAllocHeap()/1024;

    char line1[32];
    snprintf(line1, sizeof(line1), "fre|max:%lu|%lu", freeHeap, maxAlloc);
    const char* line2 = capturePsram();
    
    lcdi2c(line1,line2);
    /* Serial Monitor */
    // Serial.print("FreeHeap: ");
    // Serial.print(freeHeap);
    // Serial.print(" | MaxAlloc: ");
    // Serial.println(maxAlloc);
  }

  

  if (millis() - lastPub >= pubInterval) {
    lastPub = millis();
    allPublishStatus(suhu,kelembapan,ldrRead,clientID,login, user);
  }

  delay(200);
}
