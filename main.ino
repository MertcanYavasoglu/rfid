#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>

// GİRİŞ okuyucu: SDA → D2 (GPIO 2), RST → GPIO 22
#define SS_PIN_1 2
#define RST_PIN_1 22

// ÇIKIŞ okuyucu: SDA → D4 (GPIO 4), RST → GPIO 21
#define SS_PIN_2 4
#define RST_PIN_2 21

#define RELAY_PIN 13


const char* ssid = "ssid";
const char* password = "pass";

const char* googleScriptURL = "GOOGLE SHEETS LINKI TO THE HERE"

MFRC522 rfid1(SS_PIN_1, RST_PIN_1);
MFRC522 rfid2(SS_PIN_2, RST_PIN_2);

const byte ADMIN_CARDS[][4] = {
  {0x7B, 0x69, 0xF8, 0x11},
  {0x7B, 0x16, 0x01, 0x11}
};
const int ADMIN_CARD_COUNT = sizeof(ADMIN_CARDS) / sizeof(ADMIN_CARDS[0]);

const byte GUEST_CARDS[][4] = {
  {0x6B, 0xFF, 0x0B, 0x11},
  {0x7B, 0xA2, 0x40, 0x11}
};
const int GUEST_CARD_COUNT = sizeof(GUEST_CARDS) / sizeof(GUEST_CARDS[0]);

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // SPI pinleri: SCK=18, MISO=19, MOSI=23
  SPI.begin(18, 19, 23); // ESP32 için manuel SPI başlat

  rfid1.PCD_Init();
  rfid2.PCD_Init();

  rfid1.PCD_DumpVersionToSerial();
  rfid2.PCD_DumpVersionToSerial();

  Serial.println("RFID sistemi başlatıldı.");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void sendToGoogleSheets(String location, String uid, String timestamp, String role) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(googleScriptURL);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"location\":\"" + location + "\",\"uid\":\"" + uid + "\",\"timestamp\":\"" + timestamp + "\",\"role\":\"" + role + "\"}";
    
    int code = http.POST(payload);
    String response = http.getString();

    Serial.println("HTTP Response Code: " + String(code));
    Serial.println("Response Body: " + response);

    http.end();
  }
}


bool compareUID(byte *uid, const byte *ref) {
  for (byte i = 0; i < 4; i++) {
    if (uid[i] != ref[i]) return false;
  }
  return true;
}

bool isAdmin(byte *uid) {
  for (int i = 0; i < ADMIN_CARD_COUNT; i++) {
    if (compareUID(uid, ADMIN_CARDS[i])) return true;
  }
  return false;
}

bool isGuest(byte *uid) {
  for (int i = 0; i < GUEST_CARD_COUNT; i++) {
    if (compareUID(uid, GUEST_CARDS[i])) return true;
  }
  return false;
}

void printUID(byte *uid, byte size) {
  for (byte i = 0; i < size; i++) {
    if (uid[i] < 0x10) Serial.print("0");
    Serial.print(uid[i], HEX);
    if (i < size - 1) Serial.print(":");
  }
}

void handleCard(MFRC522 &reader, const char *locationLabel) {
  if (reader.PICC_IsNewCardPresent() && reader.PICC_ReadCardSerial()) {
    // Convert UID to String
    String uidStr = getUIDString(reader.uid.uidByte, reader.uid.size);

    // Determine role
    String role = "UNKNOWN";
    if (isAdmin(reader.uid.uidByte)) {
      role = "ADMIN";
      activateRelay();
    } else if (isGuest(reader.uid.uidByte)) {
      role = "GUEST";
      activateRelay();
    }

    // Format time manually (since we’re not using RTC or NTP here)
    String timestamp = getFormattedTime(); // Custom function below

    // Debug Output
    Serial.print(locationLabel);
    Serial.print(" - UID: ");
    Serial.print(uidStr);
    Serial.print(" -> ");
    Serial.println(role);

    // Send to Google Sheets
    sendToGoogleSheets(locationLabel, uidStr, timestamp, role);

    reader.PICC_HaltA();
    delay(1000); // debounce
  }
}

String getUIDString(byte *uid, byte size) {
  String s = "";
  for (byte i = 0; i < size; i++) {
    if (uid[i] < 0x10) s += "0";
    s += String(uid[i], HEX);
    if (i < size - 1) s += ":";
  }
  return s;
}

// Simple timestamp (fallback) using millis()
String getFormattedTime() {
  unsigned long now = millis() / 1000;
  unsigned long seconds = now % 60;
  unsigned long minutes = (now / 60) % 60;
  unsigned long hours = (now / 3600) % 24;

  char buffer[20];
  sprintf(buffer, "%02lu:%02lu:%02lu", hours, minutes, seconds);
  return String(buffer);
}

void activateRelay() {
  digitalWrite(RELAY_PIN, LOW);
  delay(5000);
  digitalWrite(RELAY_PIN, HIGH);
}

void loop() {
  handleCard(rfid1, "GIRIS");
  handleCard(rfid2, "CIKIS");
}