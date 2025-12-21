#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Fingerprint.h>

/* ================= WIFI ================= */
#define WIFI_SSID "Bubu"  // name wifi
#define WIFI_PASS "28052000"  // password wifi


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

// connect wifi
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");
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
  Serial.println(httpCode);
  if (httpCode!= 200) {
    http.end();
    return false;
  }
   String res = http.getString();
   Serial.println(res);
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

}

void enrollFingerprint() {
  Serial.println("Dat tay lan 1");

  int p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    delay(200);
  }

  finger.image2Tz(1);
  Serial.println("Nhat tay ra");
  delay(2000);

  Serial.println("Dat tay lan 2");
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    delay(200);
  }

  finger.image2Tz(2);

  if (finger.createModel() != FINGERPRINT_OK) {
    Serial.println("Tao mau that bai");
    sendEnrollResult(false);
    currentMode = "NONE";
    return;
  }

  if (finger.storeModel(enrollId) == FINGERPRINT_OK) {
    Serial.println("Dang ky thanh cong");
    sendEnrollResult(true);
  } else {
    Serial.println("Luu that bai");
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

  http.POST(body);
 String res = http.getString();
  Serial.println(res);

  http.end();
}

void scanFingerprint() {
  int p = finger.getImage();
  if (p != FINGERPRINT_OK) return;

  if (finger.image2Tz() != FINGERPRINT_OK) return;

  p = finger.fingerSearch();
  if (p != FINGERPRINT_OK) {
    Serial.println("Khong trung khop");
    return;
  }

  Serial.print("Match ID: ");
  Serial.println(finger.fingerID);

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
    Serial.println("Khong tim thay AS608");
    while (1);
  }
  // clearAllFingerprints();

  connectWiFi();
  Serial.println("Ready");
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