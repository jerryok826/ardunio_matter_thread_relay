#include <Matter.h>
#include <MatterOnOffPluginUnit.h>
#include <openthread/thread.h>
#include <openthread/instance.h>

// Include the fast ASCII OLED driver and the hardware QR engine
#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#include "qrcode.h" 

// FORCE NEW HARDWARE IDENTITY (Bypasses the iOS Bluetooth Cache Blacklist)
#define CHIP_DEVICE_CONFIG_DEVICE_VENDOR_ID   0xFFF2  
#define CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_ID  0x8001  

// =========================================================================
// MANUAL CODE VERSION TRACKER (OLED Clean RF Build - Bracket Restored)
// =========================================================================
const char* CODE_CREATION_TIMESTAMP = "Friday May 22, 2026 - 02:30 PM"; 
// =========================================================================

// Pin definitions
const int relayPins[] = {D2, D3, D4, D5};
const int buttonPins[] = {D6, D7, D8, D9};
const int ledPins[]   = {D10, D11, D12, A0}; 
const int buzzerPin = D13;

bool relayStates[] = {LOW, LOW, LOW, LOW};
bool shadowMatterStates[] = {LOW, LOW, LOW, LOW};
bool bootSyncCompleted = false; 

// Volatile interrupt flags for buttons
volatile bool buttonFlags[] = {false, false, false, false};
unsigned long lastInterruptTime[] = {0, 0, 0, 0};
const unsigned long debounceDelay = 250; 

// Non-blocking timer loops
unsigned long lastNetworkCheckTime = 0;
const unsigned long networkCheckInterval = 3000; // FAST DISP REFRESH: Poll RF signal every 3 seconds for walk-testing
unsigned long lastBlinkTime = 0;
bool blinkState = false;
unsigned long lastNetworkPollTime = 0;
const unsigned long networkPollInterval = 50; 

// Instantiate the OLED object
SSD1306AsciiWire oled;

MatterOnOffPluginUnit matterRelay1;
MatterOnOffPluginUnit matterRelay2;
MatterOnOffPluginUnit matterRelay3;
MatterOnOffPluginUnit matterRelay4;

void updateOLEDUI(); // Forward declaration for function availability

// --- HARDWARE INTERRUPT SERVICE ROUTINES (ISRs) ---
void pin6_ISR() {   if (millis() - lastInterruptTime[0] > debounceDelay) { buttonFlags[0] = true; lastInterruptTime[0] = millis(); } }
void pin7_ISR() {   if (millis() - lastInterruptTime[1] > debounceDelay) { buttonFlags[1] = true; lastInterruptTime[1] = millis(); } }
void pin8_ISR() {   if (millis() - lastInterruptTime[2] > debounceDelay) { buttonFlags[2] = true; lastInterruptTime[2] = millis(); } }
void pin9_ISR() {   if (millis() - lastInterruptTime[3] > debounceDelay) { buttonFlags[3] = true; lastInterruptTime[3] = millis(); } }

void logAndSwitch(int index, bool newState, const char* source) {
  relayStates[index] = newState;
  digitalWrite(relayPins[index], newState);
  digitalWrite(ledPins[index], newState); 
  
  Serial.print("["); Serial.print(source); Serial.print("] Relay ");
  Serial.print(index + 1); Serial.println(newState ? " -> ON" : " -> OFF");
  
  updateOLEDUI();
}

// Call this inside your 10-second factory reset button while loop
void playResetBeep() {
  digitalWrite(buzzerPin, HIGH);
  delay(50); // Small blocking delays are safe during an intentional hard reboot sequence
  digitalWrite(buzzerPin, LOW);
}

// Call this inside loop() immediately when the pairing state changes
void playPairingSuccessBeep() {
  digitalWrite(buzzerPin, HIGH); delay(100);
  digitalWrite(buzzerPin, LOW);  delay(50);
  digitalWrite(buzzerPin, HIGH); delay(100);
  digitalWrite(buzzerPin, LOW);
}

bool isThreadMeshActive() {
  otInstance* instance = otInstanceInitSingle();
  if (instance == NULL) return false;
  otDeviceRole role = otThreadGetDeviceRole(instance);
  return (role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_ROUTER);
}

void updateNetworkStatusLED() {
  if (Matter.isDeviceCommissioned()) {
    digitalWrite(LEDB, LOW); 
  } else {
    if (millis() - lastBlinkTime >= 500) { 
      lastBlinkTime = millis();
      blinkState = !blinkState;
      digitalWrite(LEDB, blinkState ? LOW : HIGH);
    }
  }
}

void drawOnboardQRCode(const char* qrDataString) {
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, qrDataString);
  
  oled.clear();
  
  for (uint8_t y = 0; y < qrcode.size; y++) {
    oled.setCursor(4, y); 
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        oled.write(219); oled.write(219); 
      } else {
        oled.write(' '); oled.write(' ');
      }
    }
    oled.println();
  }
  
  oled.setFont(Adafruit5x7);
  oled.setCursor(75, 1); oled.print("SCAN TO");
  oled.setCursor(75, 2); oled.print("PAIR");
  oled.setCursor(75, 4); oled.print("CODE:");
  oled.setCursor(75, 5); oled.print("3497");
  oled.setCursor(75, 6); oled.print("0259");
  oled.setCursor(75, 7); oled.print("332");
}

void updateOLEDUI() {
  if (!Matter.isDeviceCommissioned()) {
    return; 
  }
  
  oled.clear();
  oled.setFont(Adafruit5x7);
  oled.setCol(0); oled.println("--- SYSTEM STATUS ---");
  oled.setCol(0); oled.println("Status: PAIRED");
  
  otInstance* instance = otInstanceInitSingle();
  int8_t rssiValue = -127; 
  
  if (instance != NULL) {
    otDeviceRole role = otThreadGetDeviceRole(instance);
    oled.setCol(0); oled.print("Role: ");
    
    if (role == OT_DEVICE_ROLE_DETACHED) {
      oled.println("SCANNING...");
    } else if (role == OT_DEVICE_ROLE_CHILD) {
      oled.println("MESH CHILD");
      otThreadGetParentLastRssi(instance, &rssiValue); 
    } else if (role == OT_DEVICE_ROLE_ROUTER) {
      oled.println("MESH ROUTER");
      otNeighborInfoIterator iterator = OT_NEIGHBOR_INFO_ITERATOR_INIT;
      otNeighborInfo neighborInfo;
      if (otThreadGetNextNeighborInfo(instance, &iterator, &neighborInfo) == OT_ERROR_NONE) {
         rssiValue = neighborInfo.mAverageRssi; 
      }
    } else {
      oled.println("OFFLINE");
    }
    
    oled.setCol(0); oled.print("Signal: ");
    if (!isThreadMeshActive()) {
      oled.println("LOST LINK");
    } else {
      oled.print(rssiValue); oled.print(" dBm (");
      if (rssiValue >= -55) oled.println("Max)");
      else if (rssiValue >= -72) oled.println("Good)");
      else if (rssiValue >= -85) oled.println("Weak)");
      else oled.println("Drop)");
    }
  }
  
  oled.println("");
  oled.setCol(0);
  oled.print("R1:"); oled.print(relayStates[0] ? "ON " : "OFF");
  oled.print(" R2:"); oled.print(relayStates[1] ? "ON " : "OFF");
  oled.print(" R3:"); oled.print(relayStates[2] ? "ON " : "OFF");
  oled.print(" R4:"); oled.print(relayStates[3] ? "ON " : "OFF");
}

void setup() {
  delay(3000); 
  Serial.begin(115200);
  playPairingSuccessBeep();  // start beep

  Wire.begin();
  Wire.setClock(400000); 
  
  // FIXED: Changed the core configuration driver profile from Adafruit128x64 to SH1106_128x64
  // This automatically sets the correct 2-pixel internal RAM offset layout parameters
  oled.begin(&SH1106_128x64, 0x3C); 
 
 // oled.begin(&Adafruit128x64, 0x3C);

  oled.clear();
  oled.setFont(Adafruit5x7);
  oled.println("BOOTING HARDWARE...");

  for (int i = 0; i < 4; i++) {
    digitalWrite(relayPins[i], LOW);
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
    pinMode(ledPins[i], OUTPUT);
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  pinMode(BTN_BUILTIN, INPUT_PULLUP);
  pinMode(LEDR, OUTPUT); pinMode(LEDB, OUTPUT);     
  digitalWrite(LEDR, HIGH); digitalWrite(LEDB, HIGH);  

  attachInterrupt(digitalPinToInterrupt(D6), pin6_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(D7), pin7_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(D8), pin8_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(D9), pin9_ISR, FALLING);

  Matter.begin();
  matterRelay1.begin(); 
  matterRelay2.begin(); 
  matterRelay3.begin(); 
  matterRelay4.begin(); 

//  playResetBeep();
//  delay(2000); 
//  playPairingSuccessBeep();  // start beep
}

void loop() {
  unsigned long currentMillis = millis();

  if (!bootSyncCompleted && currentMillis > 4000) {
    bootSyncCompleted = true;
    
    shadowMatterStates[0] = relayStates[0] = matterRelay1.get_onoff();
    shadowMatterStates[1] = relayStates[1] = matterRelay2.get_onoff();
    shadowMatterStates[2] = relayStates[2] = matterRelay3.get_onoff();
    shadowMatterStates[3] = relayStates[3] = matterRelay4.get_onoff();
    
    if (!Matter.isDeviceCommissioned()) {
      String qrPayload = Matter.getOnboardingQRCodeUrl();
      if (qrPayload.indexOf("data=") != -1) {
         qrPayload = qrPayload.substring(qrPayload.indexOf("data=") + 5);
      } else {
         qrPayload = "MT:Y.1X040000KA0648300"; 
      }
      drawOnboardQRCode(qrPayload.c_str());
    } else {
      updateOLEDUI();
    }
    
    for (int i = 0; i < 4; i++) {
      logAndSwitch(i, relayStates[i], "BOOT-SYNC");
    }
  }

  if (bootSyncCompleted) {
    updateNetworkStatusLED(); 

    if (currentMillis - lastNetworkPollTime >= networkPollInterval) {
      lastNetworkPollTime = currentMillis;

      bool mState1 = matterRelay1.get_onoff(); 
      bool mState2 = matterRelay2.get_onoff();
      bool mState3 = matterRelay3.get_onoff();
      bool mState4 = matterRelay4.get_onoff();

      bool changeDetected = false;
      if (mState1 != shadowMatterStates[0]) { shadowMatterStates[0] = mState1; logAndSwitch(0, mState1, "REMOTE"); changeDetected = true; }
      if (mState2 != shadowMatterStates[1]) { shadowMatterStates[1] = mState2; logAndSwitch(1, mState2, "REMOTE"); changeDetected = true; }
      if (mState3 != shadowMatterStates[2]) { shadowMatterStates[2] = mState3; logAndSwitch(2, mState3, "REMOTE"); changeDetected = true; }
      if (mState4 != shadowMatterStates[3]) { shadowMatterStates[3] = mState4; logAndSwitch(3, mState4, "REMOTE"); changeDetected = true; }
      
      if (changeDetected) updateOLEDUI();
    }
    
    if (currentMillis - lastNetworkCheckTime >= networkCheckInterval) {
      lastNetworkCheckTime = currentMillis;
      updateOLEDUI();
    }

    for (int i = 0; i < 4; i++) {
      if (buttonFlags[i]) {
        buttonFlags[i] = false; 
        bool nextState = !relayStates[i];
        shadowMatterStates[i] = nextState;
        logAndSwitch(i, nextState, "LOCAL SWITCH");
        
        if (i == 0) matterRelay1.set_onoff(nextState);
        if (i == 1) matterRelay2.set_onoff(nextState);
        if (i == 2) matterRelay3.set_onoff(nextState);
        if (i == 3) matterRelay4.set_onoff(nextState);
      }
    }
  }

  if (digitalRead(BTN_BUILTIN) == LOW) {
    unsigned long startTime = millis();
    oled.clear(); oled.println("WIPING MEMORY...");
    while (digitalRead(BTN_BUILTIN) == LOW) {
      if ((millis() - startTime) / 1000 >= 10) {
        Matter.decommission();
        delay(1000);
        NVIC_SystemReset(); 
        break;
      }
      digitalWrite(LEDR, !digitalRead(LEDR)); delay(100);
    }
    digitalWrite(LEDR, HIGH); 
    updateOLEDUI();
  }
}
