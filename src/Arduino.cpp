#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// PIN
int trig = 9;
int echo = 10;
int buzzer = 8;
int relay = 7;

// Variabel
long durasi;
int jarak;

unsigned long lastTrigger = 0;
unsigned long lastScroll = 0;
unsigned long lastSound = 0;
unsigned long buzzerTimer = 0;
unsigned long doorOpenedTime = 0;

size_t scrollIndex = 0; // FIX WARNING

bool doorOpen = false;
int buzzerStep = 0;

String text = ">> Pintu Tertutup Silakan Mendekat <<";

// ======================
// SETUP
// ======================
void setup()
{
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(relay, OUTPUT);

  digitalWrite(relay, LOW);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("Sistem Pintu");

  lcd.setCursor(0,1);
  lcd.print("Otomatis");

  delay(2000);
  lcd.clear();
}

// ======================
// LOOP
// ======================
void loop()
{
  unsigned long now = millis();

  // ======================
  // BACA SENSOR (100 ms)
  // ======================
  if(now - lastTrigger >= 100)
  {
    lastTrigger = now;

    digitalWrite(trig, LOW);
    delayMicroseconds(2);

    digitalWrite(trig, HIGH);
    delayMicroseconds(10);

    digitalWrite(trig, LOW);

    durasi = pulseIn(echo, HIGH);
    jarak = durasi * 0.034 / 2;
  }

  // ======================
  // ADA ORANG
  // ======================
  if(jarak <= 15)
  {
    doorOpen = true;
    doorOpenedTime = now;

    digitalWrite(relay, HIGH);

    // MULAI POLA BUZZER
    if(now - lastSound >= 800)
    {
      lastSound = now;
      buzzerStep = 1;
      buzzerTimer = now;
    }

    lcd.setCursor(0,0);
    lcd.print("Selamat Datang ");

    lcd.setCursor(0,1);
    lcd.print("di Lab 3       ");
  }

  // ======================
  // TIDAK ADA ORANG
  // ======================
  else
  {
    // TAHAN PINTU 2 DETIK
    if (doorOpen && (now - doorOpenedTime <= 2000))
    {
      digitalWrite(relay, HIGH);
    }
    else
    {
      doorOpen = false;
      digitalWrite(relay, LOW);
    }

    // MATIKAN BUZZER
    buzzerStep = 0;
    noTone(buzzer);

    // SCROLL TEXT AMAN
    if(now - lastScroll >= 300)
    {
      lastScroll = now;

      lcd.setCursor(0,0);

      if(text.length() > 16)
      {
        lcd.print(text.substring(scrollIndex, scrollIndex + 16));

        scrollIndex++;

        if(scrollIndex > text.length() - 16)
        {
          scrollIndex = 0;
        }
      }
      else
      {
        lcd.print(text);
        scrollIndex = 0;
      }
    }

    lcd.setCursor(0,1);
    lcd.print("Jarak: ");
    lcd.print(jarak);
    lcd.print(" cm   ");
  }

  // ======================
  // POLA BUZZER NON-BLOCKING
  // ======================
  if(buzzerStep == 1)
  {
    tone(buzzer, 1500);
    if(now - buzzerTimer >= 100)
    {
      buzzerTimer = now;
      buzzerStep = 2;
      noTone(buzzer);
    }
  }
  else if(buzzerStep == 2)
  {
    if(now - buzzerTimer >= 50)
    {
      buzzerTimer = now;
      buzzerStep = 3;
    }
  }
  else if(buzzerStep == 3)
  {
    tone(buzzer, 1000);
    if(now - buzzerTimer >= 150)
    {
      buzzerStep = 0;
      noTone(buzzer);
    }
  }
}