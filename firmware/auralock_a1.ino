/*
 * ╔════════════════════════════════════════════════════════════╗
 * ║          AURALOCK ESP32 RFID Reader System                 ║
 * ║          CODE4UTECH CONSULTANCY PVT. LTD.                  ║
 * ║                    Version 1.2.2 FIXED                     ║
 * ╚════════════════════════════════════════════════════════════╝
 */

#include 
#include 
#include 
#include 
#include 

// ════════════════════════════════════════════════════════════
// DEVICE INFORMATION
// ════════════════════════════════════════════════════════════
#define DEVICE_MODEL   "AURALOCK A1"
#define FIRMWARE_VER   "v1.2.2"
#define DEVICE_TYPE    "RFID_READER"

// ════════════════════════════════════════════════════════════
// HARDWARE PIN CONFIGURATION
// ════════════════════════════════════════════════════════════
#define SS_PIN   5
#define RST_PIN  22
#define LED_PIN  2

// ════════════════════════════════════════════════════════════
// PERFORMANCE SETTINGS
// ════════════════════════════════════════════════════════════
const unsigned long SCAN_DELAY = 50;
const unsigned long DEBOUNCE_TIME = 800;
const unsigned long SPI_SPEED = 10000000;
const unsigned long HEARTBEAT_INTERVAL = 60000;
const unsigned long WIFI_TIMEOUT = 25000;  // 25 seconds

// ════════════════════════════════════════════════════════════
// API ENDPOINTS
// ════════════════════════════════════════════════════════════
const char* SERVER_URL = "https://uid.auralock.in/api/scan.php";
const char* HEARTBEAT_URL = "https://uid.auralock.in/api/heartbeat.php";

// ════════════════════════════════════════════════════════════
// GLOBAL OBJECTS
// ════════════════════════════════════════════════════════════
MFRC522 rfid(SS_PIN, RST_PIN);
Preferences prefs;
HTTPClient http;

// ════════════════════════════════════════════════════════════
// GLOBAL VARIABLES
// ════════════════════════════════════════════════════════════
String deviceUid = "AURA-UNCONFIGURED";
String deviceToken = "";
String wifiSSID = "";
String wifiPass = "";

unsigned long lastScanTime = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastReconnectAttempt = 0;
unsigned long cardCount = 0;
String lastCardUID = "";
unsigned long lastCardTime = 0;
bool systemReady = false;

// ════════════════════════════════════════════════════════════
// FORWARD DECLARATIONS
// ════════════════════════════════════════════════════════════
void displayBanner();
void initializeHardware();
void initializeRFID();
void loadConfiguration();
bool isConfigured();
void connectWiFi();
void scanCard();
String getCardUID();
String getLast5Digits(String uid);
void displayCardInfo(String uid);
String getCardType();
void sendScan(String cardUid);
void sendHeartbeat();
void checkSerialCommands();
void handleConfig(String cmd);
void printInfo();
void resetDevice();
void displayReadyMessage();

// ════════════════════════════════════════════════════════════
// SETUP FUNCTION
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  displayBanner();
  initializeHardware();
  initializeRFID();
  loadConfiguration();
  
  if (isConfigured()) {
    Serial.println("\n[INFO] Device is configured");
    connectWiFi();
  } else {
    Serial.println("\n[WARN] ⚠ Device NOT configured!");
    Serial.println("[WARN] Please use USB Setup Tool");
    Serial.println("[WARN] Or send CONFIG command via Serial");
    Serial.println("\n[EXAMPLE] CONFIG:YourSSID,YourPassword,AURA-SEC-TOKEN,AURA-A1-R-001");
  }
  
  displayReadyMessage();
  systemReady = true;
}

// ════════════════════════════════════════════════════════════
// MAIN LOOP
// ════════════════════════════════════════════════════════════
void loop() {
  if (!systemReady) return;
  
  unsigned long now = millis();
  
  checkSerialCommands();
  
  if (WiFi.status() == WL_CONNECTED) {
    if (now - lastHeartbeat > HEARTBEAT_INTERVAL) {
      sendHeartbeat();
      lastHeartbeat = now;
    }
  } else if (isConfigured() && (now - lastReconnectAttempt > 30000)) {
    Serial.println("\n[WiFi] Attempting reconnection...");
    connectWiFi();
    lastReconnectAttempt = now;
  }
  
  if (now - lastScanTime >= SCAN_DELAY) {
    lastScanTime = now;
    scanCard();
  }
}

// ════════════════════════════════════════════════════════════
// STARTUP BANNER
// ════════════════════════════════════════════════════════════
void displayBanner() {
  Serial.println("\n╔════════════════════════════════════════════════════════════╗");
  Serial.println("║        CODE4UTECH CONSULTANCY PVT. LTD.                   ║");
  Serial.println("║           AURALOCK RFID READER SYSTEM                     ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝");
  Serial.println("\n  Model:    " + String(DEVICE_MODEL));
  Serial.println("  Firmware: " + String(FIRMWARE_VER));
  Serial.println("\n════════════════════════════════════════════════════════════");
}

// ════════════════════════════════════════════════════════════
// HARDWARE INITIALIZATION
// ════════════════════════════════════════════════════════════
void initializeHardware() {
  Serial.println("\n[INIT] Hardware initialization...");
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  for(int i=0; i<3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
    delay(50);
  }
  Serial.println("[✓] Hardware ready");
}

// ════════════════════════════════════════════════════════════
// RFID INITIALIZATION
// ════════════════════════════════════════════════════════════
void initializeRFID() {
  Serial.println("\n[INIT] RFID module initialization...");
  
  SPI.begin();
  SPI.setFrequency(SPI_SPEED);
  
  rfid.PCD_Init();
  delay(50);
  
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);
  rfid.PCD_WriteRegister(rfid.RFCfgReg, (0x07<<4));
  
  byte value = rfid.PCD_ReadRegister(rfid.TxControlReg);
  if ((value & 0x03) != 0x03) {
    rfid.PCD_WriteRegister(rfid.TxControlReg, value | 0x03);
  }
  
  byte version = rfid.PCD_ReadRegister(rfid.VersionReg);
  if (version == 0x00 || version == 0xFF) {
    Serial.println("[✗] RFID ERROR: Module not found!");
    while(1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
  }
  
  Serial.print("[✓] RFID ready (v0x");
  Serial.print(version, HEX);
  Serial.println(")");
  Serial.println("[✓] Antenna: MAX gain");
  Serial.println("[✓] Speed: ULTRA FAST");
}

// ════════════════════════════════════════════════════════════
// LOAD CONFIGURATION
// ════════════════════════════════════════════════════════════
void loadConfiguration() {
  Serial.println("\n[CONFIG] Loading from flash memory...");
  
  prefs.begin("auralock", false);
  wifiSSID = prefs.getString("ssid", "");
  wifiPass = prefs.getString("pass", "");
  deviceToken = prefs.getString("token", "");
  deviceUid = prefs.getString("uid", "AURA-UNCONFIGURED");
  
  Serial.println("[CONFIG] Device UID: " + deviceUid);
  
  if (wifiSSID != "") {
    Serial.println("[CONFIG] WiFi SSID: " + wifiSSID);
    Serial.println("[CONFIG] WiFi Pass: " + String(wifiPass.length() > 0 ? "••••••••" : "(none)"));
  } else {
    Serial.println("[CONFIG] WiFi: NOT CONFIGURED");
  }
  
  if (deviceToken != "") {
    Serial.println("[CONFIG] Token: " + deviceToken.substring(0, 15) + "...");
  }
}

// ════════════════════════════════════════════════════════════
// CHECK IF CONFIGURED
// ════════════════════════════════════════════════════════════
bool isConfigured() {
  return (wifiSSID != "" && 
          deviceToken != "" && 
          deviceUid != "AURA-UNCONFIGURED" &&
          deviceUid.startsWith("AURA-A1-R-") &&
          deviceUid.length() == 13);
}

// ════════════════════════════════════════════════════════════
// CONNECT TO WIFI
// ════════════════════════════════════════════════════════════
void connectWiFi() {
  if (wifiSSID == "") {
    Serial.println("[WiFi] ERROR: No SSID configured!");
    return;
  }
  
  Serial.println("\n════════════════════════════════════════════════════════════");
  Serial.println("  CONNECTING TO WiFi");
  Serial.println("════════════════════════════════════════════════════════════");
  Serial.println("  SSID: " + wifiSSID);
  Serial.println("  Password: " + String(wifiPass.length() > 0 ? "••••••••" : "(none)"));
  Serial.println("════════════════════════════════════════════════════════════\n");
  
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
  
  Serial.print("[WiFi] Connecting");
  
  unsigned long startTime = millis();
  int dots = 0;
  
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startTime > WIFI_TIMEOUT) {
      Serial.println("\n\n[WiFi] ✗ CONNECTION TIMEOUT!");
      Serial.println("[WiFi] ════════════════════════════════════════");
      Serial.println("[WiFi] TROUBLESHOOTING:");
      Serial.println("[WiFi] 1. Check SSID: " + wifiSSID);
      Serial.println("[WiFi] 2. Verify password is correct");
      Serial.println("[WiFi] 3. Make sure router is nearby");
      Serial.println("[WiFi] 4. Check if WiFi is 2.4GHz (not 5GHz)");
      Serial.println("[WiFi] ════════════════════════════════════════");
      Serial.println("[WiFi] Will retry in 30 seconds...\n");
      return;
    }
    
    delay(500);
    Serial.print(".");
    dots++;
    if (dots % 40 == 0) Serial.println();
    
    checkSerialCommands();
  }
  
  Serial.println("\n\n════════════════════════════════════════════════════════════");
  Serial.println("  ✓ WiFi CONNECTED!");
  Serial.println("════════════════════════════════════════════════════════════");
  Serial.println("  IP Address: " + WiFi.localIP().toString());
  Serial.println("  Signal:     " + String(WiFi.RSSI()) + " dBm");
  Serial.println("  MAC:        " + WiFi.macAddress());
  Serial.println("════════════════════════════════════════════════════════════\n");
  
  delay(1000);
  sendHeartbeat();
}

// ════════════════════════════════════════════════════════════
// SCAN RFID CARD
// ════════════════════════════════════════════════════════════
void scanCard() {
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;
  
  unsigned long now = millis();
  String currentUID = getCardUID();
  
  if (currentUID == lastCardUID && (now - lastCardTime < DEBOUNCE_TIME)) {
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }
  
  lastCardUID = currentUID;
  lastCardTime = now;
  cardCount++;
  
  digitalWrite(LED_PIN, HIGH);
  displayCardInfo(currentUID);
  
  if (WiFi.status() == WL_CONNECTED) {
    sendScan(currentUID);
  } else {
    Serial.println("[!] Not connected to WiFi - scan not sent");
    Serial.println("[!] Device will retry WiFi connection automatically\n");
  }
  
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  digitalWrite(LED_PIN, LOW);
}

// ════════════════════════════════════════════════════════════
// GET CARD UID
// ════════════════════════════════════════════════════════════
String getCardUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

// ════════════════════════════════════════════════════════════
// GET LAST 5 DIGITS
// ════════════════════════════════════════════════════════════
String getLast5Digits(String uid) {
  if (uid.length() >= 5) {
    return uid.substring(uid.length() - 5);
  }
  return uid;
}

// ════════════════════════════════════════════════════════════
// DISPLAY CARD INFO
// ════════════════════════════════════════════════════════════
void displayCardInfo(String uid) {
  String last5 = getLast5Digits(uid);
  String cardType = getCardType();
  
  unsigned long t = millis() / 1000;
  int h = (t / 3600) % 24;
  int m = (t / 60) % 60;
  int s = t % 60;
  
  Serial.println("\n╔════════════════════════════════════════════════════════════╗");
  Serial.println("║                    CARD DETECTED                           ║");
  Serial.println("╠════════════════════════════════════════════════════════════╣");
  
  Serial.print("║ Scan #");
  Serial.print(cardCount);
  Serial.print(" │ ");
  if(h<10) Serial.print("0"); Serial.print(h); Serial.print(":");
  if(m<10) Serial.print("0"); Serial.print(m); Serial.print(":");
  if(s<10) Serial.print("0"); Serial.print(s);
  Serial.print(" │ ");
  Serial.print(cardType);
  
  int spaces = 60 - 11 - String(cardCount).length() - 15 - cardType.length();
  for(int i=0; i 0) {
    Serial.print("[HTTP] Response: ");
    Serial.println(code);
    
    if (code == 200 || code == 201) {
      Serial.println("[HTTP] ✓ Scan sent successfully!");
    } else if (code == 403) {
      Serial.println("[HTTP] ✗ Device not registered!");
    } else if (code == 401) {
      Serial.println("[HTTP] ✗ Invalid token!");
    }
  } else {
    Serial.println("[HTTP] ✗ Connection failed");
  }
  
  http.end();
  Serial.println();
}

// ════════════════════════════════════════════════════════════
// SEND HEARTBEAT
// ════════════════════════════════════════════════════════════
void sendHeartbeat() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  http.begin(HEARTBEAT_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);
  
  String payload = "{";
  payload += "\"device_id\":\"" + deviceUid + "\",";
  payload += "\"firmware\":\"" + String(FIRMWARE_VER) + "\",";
  payload += "\"model\":\"" + String(DEVICE_MODEL) + "\",";
  payload += "\"token\":\"" + deviceToken + "\",";
  payload += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  payload += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  payload += "\"uptime\":" + String(millis() / 1000) + ",";
  payload += "\"scans\":" + String(cardCount);
  payload += "}";
  
  int code = http.POST(payload);
  
  if (code == 200) {
    Serial.println("[Heartbeat] ✓ Sent");
  }
  
  http.end();
}

// ════════════════════════════════════════════════════════════
// CHECK SERIAL COMMANDS (ENHANCED)
// ════════════════════════════════════════════════════════════
void checkSerialCommands() {
  if (!Serial.available()) return;
  
  // Wait a bit for complete command to arrive
  delay(50);
  
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  
  if (cmd.length() == 0) return;
  
  Serial.println("\n[CMD] Received: " + cmd);
  Serial.flush(); // Ensure output is sent
  
  if (cmd.startsWith("CONFIG:")) {
    handleConfig(cmd);
  } else if (cmd == "GET_INFO" || cmd == "INFO") {
    printInfo();
  } else if (cmd == "STATUS") {
    Serial.println("STATUS:OK");
    Serial.flush();
  } else if (cmd == "RESET") {
    resetDevice();
  } else if (cmd == "WIFI") {
    connectWiFi();
  } else {
    Serial.println("[CMD] Unknown command: " + cmd);
    Serial.println("[CMD] Available commands: CONFIG, GET_INFO, STATUS, RESET, WIFI");
    Serial.flush();
  }
}

// ════════════════════════════════════════════════════════════
// HANDLE CONFIG COMMAND (ENHANCED)
// ════════════════════════════════════════════════════════════
void handleConfig(String cmd) {
  Serial.println("\n[CONFIG] Processing configuration...");
  Serial.flush();
  
  String data = cmd.substring(7);
  data.trim();
  
  Serial.println("[CONFIG] Raw data received (length: " + String(data.length()) + ")");
  Serial.flush();
  
  int c1 = data.indexOf(',');
  int c2 = data.indexOf(',', c1 + 1);
  int c3 = data.indexOf(',', c2 + 1);
  
  if (c1 <= 0 || c2 <= 0 || c3 <= 0) {
    Serial.println("[ERROR] Invalid format!");
    Serial.println("[ERROR] Expected: CONFIG:SSID,PASSWORD,TOKEN,UID");
    Serial.println("[ERROR] Example: CONFIG:Airtel_gaut_7578,mypass123,AURA-SEC-ABC,AURA-A1-R-001");
    Serial.flush();
    return;
  }
  
  String newSSID = data.substring(0, c1);
  String newPass = data.substring(c1 + 1, c2);
  String newToken = data.substring(c2 + 1, c3);
  String newUid = data.substring(c3 + 1);
  
  // Trim all
  newSSID.trim();
  newPass.trim();
  newToken.trim();
  newUid.trim();
  
  Serial.println("\n[CONFIG] Parsed values:");
  Serial.println("[CONFIG] SSID: " + newSSID);
  Serial.println("[CONFIG] Pass: " + String(newPass.length() > 0 ? "••••••••" : "(none)"));
  Serial.println("[CONFIG] Token: " + newToken.substring(0, 15) + "...");
  Serial.println("[CONFIG] UID: " + newUid);
  Serial.flush();
  
  // Validate UID
  if (!newUid.startsWith("AURA-A1-R-") || newUid.length() != 13) {
    Serial.println("\n[ERROR] ✗ Invalid UID format!");
    Serial.println("[ERROR] Expected: AURA-A1-R-XXX (where XXX is 001-999)");
    Serial.println("[ERROR] Received: " + newUid);
    Serial.flush();
    return;
  }
  
  // Validate SSID
  if (newSSID.length() == 0 || newSSID.length() > 32) {
    Serial.println("\n[ERROR] ✗ Invalid SSID! (must be 1-32 characters)");
    Serial.flush();
    return;
  }
  
  // Validate Token
  if (newToken.length() < 10) {
    Serial.println("\n[ERROR] ✗ Invalid token! (too short)");
    Serial.flush();
    return;
  }
  
  // Save to flash
  Serial.println("\n[CONFIG] Saving to flash memory...");
  Serial.flush();
  
  prefs.putString("ssid", newSSID);
  prefs.putString("pass", newPass);
  prefs.putString("token", newToken);
  prefs.putString("uid", newUid);
  
  // Verify saved
  String verify_ssid = prefs.getString("ssid", "");
  String verify_uid = prefs.getString("uid", "");
  
  if (verify_ssid == newSSID && verify_uid == newUid) {
    Serial.println("[CONFIG] ✓ Verification successful!");
  } else {
    Serial.println("[CONFIG] ✗ Verification failed!");
    Serial.println("[CONFIG] Saved SSID: " + verify_ssid);
    Serial.println("[CONFIG] Saved UID: " + verify_uid);
  }
  Serial.flush();
  
  Serial.println("\n════════════════════════════════════════════════════════════");
  Serial.println("  ✓ CONFIGURATION SAVED!");
  Serial.println("════════════════════════════════════════════════════════════");
  Serial.println("  Device UID:   " + newUid);
  Serial.println("  WiFi SSID:    " + newSSID);
  Serial.println("  WiFi Pass:    " + String(newPass.length() > 0 ? "••••••••" : "(none)"));
  Serial.println("  Token:        " + newToken.substring(0, 15) + "...");
  Serial.println("════════════════════════════════════════════════════════════");
  Serial.println("  RESTARTING IN 3 SECONDS...");
  Serial.println("════════════════════════════════════════════════════════════\n");
  Serial.flush();
  
  delay(3000);
  ESP.restart();
}

// ════════════════════════════════════════════════════════════
// PRINT DEVICE INFO
// ════════════════════════════════════════════════════════════
void printInfo() {
  Serial.println("\n════════════════════════════════════════════════════════════");
  Serial.println("  DEVICE INFORMATION");
  Serial.println("════════════════════════════════════════════════════════════");
  Serial.println("  Device UID:    " + deviceUid);
  Serial.println("  Model:         " + String(DEVICE_MODEL));
  Serial.println("  Firmware:      " + String(FIRMWARE_VER));
  Serial.println("  WiFi SSID:     " + (wifiSSID != "" ? wifiSSID : "Not configured"));
  Serial.println("  WiFi Status:   " + String(WiFi.status() == WL_CONNECTED ? "Connected ✓" : "Disconnected ✗"));
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("  IP Address:    " + WiFi.localIP().toString());
    Serial.println("  Signal:        " + String(WiFi.RSSI()) + " dBm");
  }
  
  Serial.println("  Cards Scanned: " + String(cardCount));
  Serial.println("  Uptime:        " + String(millis() / 1000) + " sec");
  Serial.println("════════════════════════════════════════════════════════════\n");
}

// ════════════════════════════════════════════════════════════
// RESET DEVICE
// ════════════════════════════════════════════════════════════
void resetDevice() {
  Serial.println("\n════════════════════════════════════════════════════════════");
  Serial.println("  ⚠ RESETTING DEVICE");
  Serial.println("════════════════════════════════════════════════════════════");
  Serial.println("  Clearing all configuration...");
  
  prefs.clear();
  
  Serial.println("  ✓ Configuration cleared!");
  Serial.println("  Restarting device...");
  Serial.println("════════════════════════════════════════════════════════════\n");
  
  delay(2000);
  ESP.restart();
}

// ════════════════════════════════════════════════════════════
// READY MESSAGE
// ════════════════════════════════════════════════════════════
void displayReadyMessage() {
  Serial.println("\n════════════════════════════════════════════════════════════");
  Serial.println("  SYSTEM READY");
  Serial.println("════════════════════════════════════════════════════════════");
  Serial.println("  Status:      ACTIVE ✓");
  Serial.println("  Frequency:   13.56 MHz");
  Serial.println("  Range:       5-8 cm");
  Serial.println("  Speed:       50ms");
  Serial.println("  WiFi:        " + String(WiFi.status() == WL_CONNECTED ? "Connected ✓" : "Not connected ✗"));
  Serial.println("════════════════════════════════════════════════════════════");
  Serial.println("  READY TO SCAN CARDS!");
  Serial.println("════════════════════════════════════════════════════════════\n");
}

/*
 * ════════════════════════════════════════════════════════════
 * CODE4UTECH CONSULTANCY PVT. LTD.
 * ════════════════════════════════════════════════════════════
 */
