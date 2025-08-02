#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>

// RFID 1 (GİRİŞ)
#define SS_PIN_1   2
#define RST_PIN_1  22
// RFID 2 (ÇIKIŞ)
#define SS_PIN_2   4
#define RST_PIN_2  21

// Röle, Mod Butonu
#define RELAY_PIN    13
#define MODE_BUTTON  12

// instantiate your two readers here:
MFRC522 rfid1(SS_PIN_1, RST_PIN_1);
MFRC522 rfid2(SS_PIN_2, RST_PIN_2);

// WiFi + single Google Script endpoint
const char* ssid             = "nshioshi";
const char* password         = "87654321";
const char* googleScriptURL  = "https://script.google.com/macros/s/AKfycby82M32S9VsCAxgehuy-pDPeDbXAVZGyCf3uW6keuCeGHFvzX1OTrCTR8Qo4u8Vx2qIQQ/exec";

// CSV-export URL for the UIDs tab (needed by fetchUIDsFromSheet)
const char* googleUIDSheetCSV =
  "https://docs.google.com/spreadsheets/d/YOUR_SPREADSHEET_ID/"
  "gviz/tq?tqx=out:csv&sheet=UIDs";


// Statik ADMIN listesi
const byte ADMIN_CARDS[][4] = {
  {0x7B,0x69,0xF8,0x11},
  {0x7B,0x16,0x01,0x11}
};
const int ADMIN_COUNT = sizeof(ADMIN_CARDS)/sizeof(ADMIN_CARDS[0]);

// Modlar
enum Mode { DOOR_UNLOCK, UID_LEARN };
Mode currentMode = DOOR_UNLOCK;

// Öğrenilmiş UID’ler (max 100)
#define MAX_UIDS 100
String learnedUIDs[MAX_UIDS];
int learnedCount = 0;

// ——— SETUP —————————————————————————————————————————
void setup() {
  pinMode(MODE_BUTTON, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  Serial.begin(115200);

  // RFID init
  SPI.begin(18,19,23);
  rfid1.PCD_Init();
  rfid2.PCD_Init();

  // WiFi
  WiFi.begin(ssid,password);
  Serial.print("WiFi connecting");
  while(WiFi.status()!=WL_CONNECTED){
    delay(300); Serial.print(".");
  }
  Serial.println("\nWiFi OK, IP=" + WiFi.localIP().toString());

  // Başlangıçta “door” moduna geçince çek
  fetchUIDsFromSheet();
}

// ——— YARDIMCI ———————————————————————————————————————

// Butonla mod değişimi
void handleModeSwitch(){
  static bool last=HIGH;
  bool cur = digitalRead(MODE_BUTTON);
  if(last==HIGH && cur==LOW){
    currentMode = (currentMode==DOOR_UNLOCK ? UID_LEARN : DOOR_UNLOCK);
    Serial.println("** MODE -> " 
      + String(currentMode==DOOR_UNLOCK? "DOOR_UNLOCK":"UID_LEARN"));
    delay(300);
    if(currentMode==DOOR_UNLOCK) fetchUIDsFromSheet();
  }
  last=cur;
}

// CSV’den UID listeyi çek
void fetchUIDsFromSheet(){
  learnedCount = 0;
  HTTPClient http;
  http.begin(googleUIDSheetCSV);
  int code = http.GET();
  if(code==200){
    String csv = http.getString();
    int idx = 0;
    while (idx < csv.length() && learnedCount < MAX_UIDS) {
      int nl = csv.indexOf('\n', idx);
      String line = (nl > idx ? csv.substring(idx, nl) : csv.substring(idx));
      line.trim();
      idx = (nl < 0 ? csv.length() : nl + 1);
      if (line.length() > 0) {
        learnedUIDs[learnedCount++] = line;
      }
    }
    Serial.printf("Fetched %d learned UIDs\n", learnedCount);
  } else {
    Serial.println("UID CSV fetch failed: " + String(code));
  }
  http.end();
}

// UID dizi kontrolü
bool isLearnedUID(const String &uid){
  for(int i=0;i<learnedCount;i++){
    if(learnedUIDs[i] == uid) return true;
  }
  return false;
}

// UID string’e çevir
String getUIDString(byte *u, byte sz){
  String s;
  for(byte i=0;i<sz;i++){
    if(u[i]<0x10) s += "0";
    s += String(u[i], HEX);
    if(i<sz-1) s += ":";
  }
  return s;
}

// Röle tetikleme
void activateRelay(int ms=5000){
  digitalWrite(RELAY_PIN, LOW);
  delay(ms);
  digitalWrite(RELAY_PIN, HIGH);
}

// POST JSON helper
void httpPost(const char *url, String json){
  if(WiFi.status() != WL_CONNECTED) return;
  HTTPClient h;
  h.begin(url);
  h.addHeader("Content-Type","application/json");
  int c = h.POST(json);
  String r = h.getString();
  Serial.println("HTTP " + String(c) + " " + r);
  h.end();
}

// ——— MODLAR —————————————————————————————————————————

// UID öğrenme modunda
void handleUIDLearn(MFRC522 &r){
  if(!r.PICC_IsNewCardPresent() || !r.PICC_ReadCardSerial()) return;
  String uid = getUIDString(r.uid.uidByte, r.uid.size);
  Serial.println("Learn UID: " + uid);

  // single unified endpoint with LEARN operation
  String j = "{\"operation\":\"LEARN\",\"uid\":\""+uid+"\"}";
  httpPost(googleScriptURL, j);

  // local cache’e ekle
  if(!isLearnedUID(uid) && learnedCount < MAX_UIDS){
    learnedUIDs[learnedCount++] = uid;
    Serial.println("Cached new UID");
  }

  r.PICC_HaltA();
  delay(300);
}

// Kapı kilidi modu
void handleDoor(MFRC522 &r, const char* loc){
  if(!r.PICC_IsNewCardPresent() || !r.PICC_ReadCardSerial()) return;

  String uid = getUIDString(r.uid.uidByte, r.uid.size);
  String role = "NONE";

  // ADMIN
  for(int i=0;i<ADMIN_COUNT;i++){
    if(compareUID(r.uid.uidByte, ADMIN_CARDS[i])){
      role = "ADMIN";
      activateRelay();
      break;
    }
  }
  // Learned
  if(role == "NONE" && isLearnedUID(uid)){
    role = "GUEST";
    activateRelay();
  }

  // LOG operation
  String tm = getFormattedTime();
  String j = String("{\"operation\":\"LOG\",")
           + "\"location\":\""+loc+"\","
           + "\"uid\":\""+uid+"\","
           + "\"timestamp\":\""+tm+"\","
           + "\"role\":\""+role+"\"}";
  httpPost(googleScriptURL, j);

  Serial.printf("%s - %s -> %s\n", loc, uid.c_str(), role.c_str());
  r.PICC_HaltA();
  delay(300);
}

// loop
void loop(){
  handleModeSwitch();
  if(currentMode == UID_LEARN){
    handleUIDLearn(rfid1);
    handleUIDLearn(rfid2);
  } else {
    handleDoor(rfid1, "GIRIS");
    handleDoor(rfid2, "CIKIS");
  }
}

// ——— EK FONKSIYONLAR —————————————————————————————————————
bool compareUID(byte *u1, const byte *u2){
  for(byte i=0;i<4;i++) if(u1[i]!=u2[i]) return false;
  return true;
}
String getFormattedTime(){
  unsigned long t = millis()/1000;
  unsigned long ss=t%60, mm=(t/60)%60, hh=(t/3600)%24;
  char b[12]; sprintf(b,"%02lu:%02lu:%02lu",hh,mm,ss);
  return String(b);
}
