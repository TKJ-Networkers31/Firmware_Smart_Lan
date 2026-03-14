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
unsigned long lastLcd = 0;
unsigned long doorTimer = 0;

// ======== INTERVAL TIME FOR MILIS ========
const unsigned long dhtInterval = 2000;
const unsigned long ldrInterval = 500;
const unsigned long pubInterval = 2000;
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
int relay[] = {27,14,17}; //array relay pin
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
  "lab1/access"
};
const char* topicSub[] = {
  "lab1/control/login",
  "lab1/control/door",
  "lab1/control/lamp",
  "lab1/control/mode",
  "lab1/control/lock",
  "lab1/control/ac"
};

const char* codeAc[] = {
  //in progres
};

// ---------- BUFFERS ----------
// char uidBuf[32];
char jsonBuf[512];
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
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
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
  int retry = 0;

  while (!mqtt.connected() && retry < 5) {
    mqtt.connect(clientID);
    delay(500);
    retry++;
  }

  for(int i = 0; i<=4 ;i++){
    mqtt.subscribe(topicSub[i]);
  }
  lcd.clear();
  lcdi2c("connected to","broker");
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

//publish all data to broker
void allPublishStatus(float temprature, float humadity, int light, const char* client , bool login, char* user) {
  StaticJsonDocument<512> doc;
  
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
  // doc["lamp1"] = digitalRead(RELAY_LED1) ? "ON" : "OFF";
  doc["lamp2_3"] = digitalRead(RELAY_LED2_3) ? "ON" : "OFF";
  // doc["lamp3"] = digitalRead(RELAY_LED3) ? "ON" : "OFF";
  doc["lamp1_4"] = digitalRead(RELAY_LED1_4) ? "ON" : "OFF";

  size_t n = serializeJson(doc, jsonBuf, sizeof(jsonBuf));
  // Serial.println(jsonBuf);
  mqtt.publish(topicPub[0], jsonBuf, n);
}

//mqtt callback
void callback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) return;
  if (strcmp(topic, topicSub[0]) == 0 || !locked) {
    strcpy(statusAcces,doc["statusAccess"]);
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
      delay(1000);
      lcdi2c("used by : ",user);
    }else{
      lcdi2c(statusAcces,"kartu tidak ada");
      beep(1000);
    }
  }
  if (strcmp(topic, topicSub[1]) == 0 || !locked) {
    (strcmp(doc["door"], "HIGH") == 0)? digitalWrite(SELENOID,HIGH):digitalWrite(SELENOID,LOW);

  }
  if (strcmp(topic, topicSub[2]) == 0 || !locked) {
    (strcmp(doc["lamp2"], "HIGH") == 0)? digitalWrite(RELAY_LED2_3,HIGH):digitalWrite(RELAY_LED2_3,LOW);
    (strcmp(doc["lamp4"], "HIGH") == 0)? digitalWrite(RELAY_LED1_4,HIGH):digitalWrite(RELAY_LED1_4,LOW);
  }
  if(strcmp(topic, topicSub[3]) == 0){
    if(strcmp(doc["mode"],"auto")==0){
      modeAuto = true;
    }else if(strcmp(doc["mode"],"manual")==0){
      modeAuto = false;
    }else{
      //sentErrorMassage(); #coming soon
    }
  }
  if(strcmp(topic, topicSub[4]) == 0 || !login){
    if(strcmp(doc["lock"],"lock")==0){
      locked = true;
    }else if(strcmp(doc["lock"],"unlock")==0){
      locked = false;
    }else{
      //sentErrorMassage(); #coming soon
    }
  }
  if(strcmp(topic, topicSub[5]) == 0){
    //ac code in progres
  }
}

//mengirim request login
void sentLogin() {
  if (!login || (strcmp(user,"none")==0 && strcmp(idLoginNow,"none")==0)){
    lcdi2c("tempelkan kartu","untuk login");
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
    strcpy(idLoginNow, id.c_str());
    StaticJsonDocument<256> doc;
    doc["id"] = id;
    size_t n = serializeJson(doc, jsonBuf, sizeof(jsonBuf));
    mqtt.publish(topicPub[1], jsonBuf, n);
    rfid.PICC_HaltA();
  }else{
    lcdi2c("tempelkan kartu","untuk logout");
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
      size_t n = serializeJson(doc, jsonBuf, sizeof(jsonBuf));
      mqtt.publish(topicPub[1], jsonBuf, n);
      rfid.PICC_HaltA();
    }else{
      lcdi2c("kartu tidak terdaftar", "tunggu....");
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
  delay(1000);
  connectMQTT();
  mqtt.setCallback(callback);
  lcdi2c("esp 32 ready sir","ready for work");
  delay(3000);

  //deklarasi input output
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LDR, INPUT); 
  pinMode(PIN_BUZZER, OUTPUT); 
  for(int i = 0; i <= 2; i++){
    pinMode(relay[i], OUTPUT);
    digitalWrite(relay[i], LOW); 
  }
  Serial.println("ESP32 IoT Ready");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("iot is ready");
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
      return;
    }
    // char line[32];
    // snprintf(line, sizeof(line), "T:%.1fC H:%.0f%%", suhu, kelembapan);
    // Serial.println(line);
  }

  if(modeAuto){
    if(!locked){
      sentLogin();
      if(login){
        if(ldrRead <= thresholdLdr){
          for(int i = 0; i<=1; i++){
            digitalWrite(relay[i],HIGH);
          }
          delay(100);
        }else{
          for(int i = 0; i<=1; i++){
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
      }
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

  if (millis() - lastPub >= pubInterval) {
    lastPub = millis();
    allPublishStatus(suhu,kelembapan,ldrRead,clientID,login, user);
  }

  delay(200);
}
