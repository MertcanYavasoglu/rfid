#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include "RTClib.h"

// RFID 1 (GİRİŞ)
#define SS_PIN_1 2
#define RST_PIN_1 22

// RFID 2 (ÇIKIŞ)
#define SS_PIN_2 4
#define RST_PIN_2 21

MFRC522 rfid1(SS_PIN_1, RST_PIN_1);
MFRC522 rfid2(SS_PIN_2, RST_PIN_2);

// Röle
#define RELAY_PIN 13

// Saat
RTC_DS3231 rtc;

// Yetkili kartın UID'si
byte YETKILI_KART[4] = {0x7B, 0x69, 0xF8, 0x11};  // Kart UID'in

// WiFi bilgileri
const char* ssid = "test123";
const char* password = "nshioshi456";

// Sunucu (bilgisayar) IP adresi ve portu
const char* serverURL = "http://192.168.16.194:8080/log"; // IP adresi kendi bilgisayarına göre

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid1.PCD_Init();
  rfid2.PCD_Init();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  Wire.begin();
  rtc.begin();

  WiFi.begin(ssid, password);
  Serial.print("WiFi bağlanıyor");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi bağlantısı başarılı!");
}

bool compareUID(byte *uid, byte *ref) {
  for (byte i = 0; i < 4; i++) {
    if (uid[i] != ref[i]) return false;
  }
  return true;
}

void sendToServer(String text) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");

    String json = "{\"log\":\"" + text + "\"}";
    int httpResponseCode = http.POST(json);

    if (httpResponseCode > 0) {
      Serial.println("Sunucu cevabı: " + http.getString());
    } else {
      Serial.println("Gönderme başarısız.");
    }

    http.end();
  } else {
    Serial.println("WiFi bağlantısı yok!");
  }
}

void activateRelay() {
  digitalWrite(RELAY_PIN, HIGH);
  delay(5000);
  digitalWrite(RELAY_PIN, LOW);
}

void loop() {
  // GİRİŞ RFID okuyucu
  if (rfid1.PICC_IsNewCardPresent() && rfid1.PICC_ReadCardSerial()) {
    if (compareUID(rfid1.uid.uidByte, YETKILI_KART)) {
      DateTime now = rtc.now();
      String log = "GIRIS - " + String(now.timestamp());
      sendToServer(log);
      activateRelay();
    } else {
      Serial.println("Yetkisiz kart (giris okuyucu)");
    }
    rfid1.PICC_HaltA();
  }

  // ÇIKIŞ RFID okuyucu
  if (rfid2.PICC_IsNewCardPresent() && rfid2.PICC_ReadCardSerial()) {
    if (compareUID(rfid2.uid.uidByte, YETKILI_KART)) {
      DateTime now = rtc.now();
      String log = "CIKIS - " + String(now.timestamp());
      sendToServer(log);
      activateRelay();
    } else {
      Serial.println("Yetkisiz kart (cikis okuyucu)");
    }
    rfid2.PICC_HaltA();
  }
}
