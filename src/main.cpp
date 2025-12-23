#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Fingerprint.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* ================= WIFI ================= */
#define WIFI_SSID "Bubu"  // name wifi
#define WIFI_PASS "28052000"  // password wifi

/* ================= OLED ================= */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define DEVICE_ID  "ESP-01"
#define POLL_INTERVAL 5000
#define ATTENDANCE_COOLDOWN 8000
unsigned long lastPoll = 0;

HardwareSerial FingerSerial(2);
Adafruit_Fingerprint finger(&FingerSerial);

// kiem tra ip bang ipconfig
String IP_ADDRESS = "192.168.1.13";
String PORT = "3000";
String BASE_URL = "http://" + IP_ADDRESS + ":" + PORT + "/api/";

//API
String API_COMMAND = BASE_URL+ "device-command/request-enroll?" + "deviceId=" + DEVICE_ID;
String API_ENROLL_RESULT = BASE_URL+ "device-command/enroll-result";
String API_ATTENDANCE = BASE_URL+ "attendance";

unsigned long lastAttendance = 0;


/// -------------------------------------//
String currentMode = "NONE";

uint8_t enrollId = 0;

// display text
void displayText(String text) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Che do: ");
  if(currentMode == "NONE"){
    display.println("Diem danh");
  }
  else{
    display.println("Dang ky");
  }
  display.println(text);
  display.display();
}

// connect wifi
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
  displayText("Ket noi WiFi ... ");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  displayText("Ket noi thanh cong");
}

uint16_t findEmptyFingerprintID() {
  for (uint16_t id = 1; id <= 127; id++) {
    uint8_t p = finger.loadModel(id);
    if (p != FINGERPRINT_OK) {
      return id; // slot 
    }
  }
  return 0; // full
}

bool pollCommand(){
   if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  http.begin(API_COMMAND);
  int httpCode = http.GET();
  if (httpCode!= 200) {
    http.end();
    return false;
  }
   String res = http.getString();
   if(res.indexOf("ENROLL") > 0 ){
    currentMode = "ENROLL";
    int tempId = findEmptyFingerprintID();
    if(tempId == 0){
      currentMode = "NONE";
      return false;
    }
    enrollId = tempId;
    http.end();
    return true;  
   }
   currentMode = "NONE";
  http.end();
  return true;
}

void sendEnrollResult(boolean success){
    HTTPClient http;
    http.begin(API_ENROLL_RESULT);
    http.addHeader("Content-Type", "application/json");
   String body = "{";
      body += "\"deviceId\":\"" DEVICE_ID "\",";
      body += "\"fingerprintId\":" + String(enrollId) + ",";
      body += "\"status\":\"";
      body += success ? "SUCCESS\"" : "FAIL\"";
      body += "}";
      int httpCode = http.POST(body);
      Serial.println(httpCode);
      if(httpCode != 201){
        displayText("Gui that bai.");
      }
      else{
        displayText("Gui thanh cong.");
      }
      Serial.println(httpCode);
      http.end();
      delay(1000);
}

void enrollFingerprint() {
  displayText("Dat tay lan 1");
  int p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    delay(200);
  }

  finger.image2Tz(1);
  displayText("Nhat tay ra");
  delay(2000);

 displayText("Dat tay lan 2");
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    delay(200);
  }

  finger.image2Tz(2);

  if (finger.createModel() != FINGERPRINT_OK) {
    displayText("Tao mau that bai");
    sendEnrollResult(false);
    currentMode = "NONE";
    return;
  }

  if (finger.storeModel(enrollId) == FINGERPRINT_OK) {
    displayText("Dang ky thanh cong");
    sendEnrollResult(true);
  } else {
    displayText("Luu that bai");
    sendEnrollResult(false);
  }

  currentMode = "NONE";
}

void sendAttendance(uint8_t id) {
  if (millis() - lastAttendance < ATTENDANCE_COOLDOWN) return;
  lastAttendance = millis();

  HTTPClient http;
  http.begin(API_ATTENDANCE);
  http.addHeader("Content-Type", "application/json");

  String body = "{";
  body += "\"deviceId\":\"" DEVICE_ID "\",";
  body += "\"fingerprintId\":" + String(id);
  body += "}";


 int httpCode = http.POST(body);
Serial.println(httpCode);
    if(httpCode != 201){
      displayText("Gui that bai.");
    }
    else{
      displayText("Gui thanh cong.");
    }
    http.end();
    delay(1000);
}

void scanFingerprint() {
  int p = finger.getImage();
  if (p != FINGERPRINT_OK) return;

  if (finger.image2Tz() != FINGERPRINT_OK) return;

  p = finger.fingerSearch();
  if (p != FINGERPRINT_OK) {
    displayText("Khong trung khop");
    return;
  }

  sendAttendance(finger.fingerID);
}

void clearAllFingerprints() {
  Serial.println("⚠️ Clearing fingerprint database...");

  uint8_t p = finger.emptyDatabase();

  if (p == FINGERPRINT_OK) {
    Serial.println("All fingerprints deleted");
  } else {
    Serial.print("Failed, error code: ");
    Serial.println(p);
  }
}


void setup() {
  Serial.begin(115200);

  FingerSerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);

  if (!finger.verifyPassword()) {
    displayText("Khong tim thay AS608");
    while (1);
  }
  // clearAllFingerprints();

   Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("Khong tim thay OLED");
    while (true);
  }
   display.clearDisplay();

  connectWiFi();
}

void loop() {

  if(currentMode == "NONE"){
    scanFingerprint();
    if(millis() - lastPoll > POLL_INTERVAL){
      lastPoll = millis();
      pollCommand();
    }
  }
  if(currentMode == "ENROLL"){
    enrollFingerprint();
  }
  delay(500);
}