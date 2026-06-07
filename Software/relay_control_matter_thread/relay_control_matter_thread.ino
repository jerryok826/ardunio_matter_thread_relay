#include <Matter.h>
#include <MatterOnOffPluginUnit.h>
#include <openthread/thread.h>
#include <openthread/instance.h>

#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#include "qrcode.h"

#define CHIP_DEVICE_CONFIG_DEVICE_VENDOR_ID   0xFFF2
#define CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_ID  0x8001

const char* CODE_CREATION_TIMESTAMP = "Friday May 22, 2026 - 02:30 PM";

const int relayPins[]  = {D2, D3, D4, D5};
const int buttonPins[] = {D6, D7, D8, D9};
const int ledPins[]    = {D10, D11, D12, A0};
const int buzzerPin = D13;

bool relayStates[] = {LOW, LOW, LOW, LOW};
bool shadowMatterStates[] = {LOW, LOW, LOW, LOW};
bool bootSyncCompleted = false;

volatile bool buttonFlags[] = {false, false, false, false};
unsigned long lastInterruptTime[] = {0, 0, 0, 0};
const unsigned long debounceDelay = 250;

unsigned long lastNetworkCheckTime = 0;
const unsigned long networkCheckInterval = 3000;

unsigned long lastBlinkTime = 0;
bool blinkState = false;

unsigned long lastNetworkPollTime = 0;
const unsigned long networkPollInterval = 50;

SSD1306AsciiWire oled;

MatterOnOffPluginUnit matterRelay1;
MatterOnOffPluginUnit matterRelay2;
MatterOnOffPluginUnit matterRelay3;
MatterOnOffPluginUnit matterRelay4;

void updateOLEDUI();

void pin6_ISR() {
  if (millis() - lastInterruptTime[0] > debounceDelay) {
    buttonFlags[0] = true;
    lastInterruptTime[0] = millis();
  }
}

void pin7_ISR() {
  if (millis() - lastInterruptTime[1] > debounceDelay) {
    buttonFlags[1] = true;
    lastInterruptTime[1] = millis();
  }
}

void pin8_ISR() {
  if (millis() - lastInterruptTime[2] > debounceDelay) {
    buttonFlags[2] = true;
    lastInterruptTime[2] = millis();
  }
}

void pin9_ISR() {
  if (millis() - lastInterruptTime[3] > debounceDelay) {
    buttonFlags[3] = true;
    lastInterruptTime[3] = millis();
  }
}

void playResetBeep() {
  digitalWrite(buzzerPin, HIGH);
  delay(50);
  digitalWrite(buzzerPin, LOW);
}

void playPairingSuccessBeep() {
  digitalWrite(buzzerPin, HIGH);
  delay(100);
  digitalWrite(buzzerPin, LOW);
  delay(50);
  digitalWrite(buzzerPin, HIGH);
  delay(100);
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

void logAndSwitch(int index, bool newState, const char* source) {
  relayStates[index] = newState;

  digitalWrite(relayPins[index], newState);
  digitalWrite(ledPins[index], newState);

  Serial.print("[");
  Serial.print(source);
  Serial.print("] Relay ");
  Serial.print(index + 1);
  Serial.println(newState ? " -> ON" : " -> OFF");

  updateOLEDUI();
}

// ======================================================================
// FIXED QR DRAWING FUNCTION
//
// Keeps SSD1306Ascii.
// Does not use Adafruit_GFX.
// Draws raw pixels only during pairing.
// QR version 3 = 29 x 29 modules.
// Scale 2 = 58 x 58 pixels, fits 128 x 64 OLED.
// ======================================================================
void drawOnboardQRCode(const char* qrDataString) {
  QRCode qrcode;

  const uint8_t qrVersion = 3;
  const uint8_t scale = 2;

  const uint8_t qrX = 2;
  const uint8_t qrY = 3;

  uint8_t qrcodeData[qrcode_getBufferSize(qrVersion)];
  qrcode_initText(&qrcode, qrcodeData, qrVersion, ECC_LOW, qrDataString);

  oled.clear();

  const uint8_t qrPixelSize = qrcode.size * scale;

  for (uint8_t page = 0; page < 8; page++) {
    oled.setCol(qrX);
    oled.setRow(page);

    for (uint8_t px = 0; px < qrPixelSize; px++) {
      uint8_t dataByte = 0;

      for (uint8_t bit = 0; bit < 8; bit++) {
        uint8_t py = page * 8 + bit;

        if (py >= qrY && py < qrY + qrPixelSize) {
          uint8_t qrModuleX = px / scale;
          uint8_t qrModuleY = (py - qrY) / scale;

          if (qrcode_getModule(&qrcode, qrModuleX, qrModuleY)) {
            dataByte |= (1 << bit);
          }
        }
      }

      oled.ssd1306WriteRam(dataByte);
    }

    delay(1);
  }

  oled.setFont(Adafruit5x7);

  oled.setCursor(70, 0);
  oled.print("SCAN");

  oled.setCursor(70, 1);
  oled.print("TO PAIR");

  oled.setCursor(70, 3);
  oled.print("CODE:");

  oled.setCursor(70, 4);
  oled.print("3497");

  oled.setCursor(70, 5);
  oled.print("0259");

  oled.setCursor(70, 6);
  oled.print("332");
}

void updateOLEDUI() {
  if (!Matter.isDeviceCommissioned()) {
    return;
  }

  oled.clear();
  oled.setFont(Adafruit5x7);

  oled.setCol(0);
  oled.println("--- SYSTEM STATUS ---");

  oled.setCol(0);
  oled.println("Status: PAIRED");

  otInstance* instance = otInstanceInitSingle();
  int8_t rssiValue = -127;

  if (instance != NULL) {
    otDeviceRole role = otThreadGetDeviceRole(instance);

    oled.setCol(0);
    oled.print("Role: ");

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

    oled.setCol(0);
    oled.print("Signal: ");

    if (!isThreadMeshActive()) {
      oled.println("LOST LINK");
    } else {
      oled.print(rssiValue);
      oled.print(" dBm (");

      if (rssiValue >= -55) {
        oled.println("Max)");
      } else if (rssiValue >= -72) {
        oled.println("Good)");
      } else if (rssiValue >= -85) {
        oled.println("Weak)");
      } else {
        oled.println("Drop)");
      }
    }
  }

  oled.println("");
  oled.setCol(0);

  oled.print("R1:");
  oled.print(relayStates[0] ? "ON " : "OFF");

  oled.print(" R2:");
  oled.print(relayStates[1] ? "ON " : "OFF");

  oled.print(" R3:");
  oled.print(relayStates[2] ? "ON " : "OFF");

  oled.print(" R4:");
  oled.print(relayStates[3] ? "ON " : "OFF");
}

void setup() {
  delay(3000);
  Serial.begin(115200);

  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  playPairingSuccessBeep();

  Wire.begin();
  Wire.setClock(400000);

  oled.begin(&SH1106_128x64, 0x3C);
  oled.clear();
  oled.setFont(Adafruit5x7);
  oled.println("BOOTING HARDWARE...");

  for (int i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);

    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);

    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  pinMode(BTN_BUILTIN, INPUT_PULLUP);

  pinMode(LEDR, OUTPUT);
  pinMode(LEDB, OUTPUT);

  digitalWrite(LEDR, HIGH);
  digitalWrite(LEDB, HIGH);

  attachInterrupt(digitalPinToInterrupt(D6), pin6_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(D7), pin7_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(D8), pin8_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(D9), pin9_ISR, FALLING);

  Matter.begin();

  matterRelay1.begin();
  matterRelay2.begin();
  matterRelay3.begin();
  matterRelay4.begin();
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
      String qrUrl = Matter.getOnboardingQRCodeUrl();
      String qrPayload = qrUrl;

      if (qrPayload.indexOf("data=") != -1) {
        qrPayload = qrPayload.substring(qrPayload.indexOf("data=") + 5);
      } else {
        qrPayload = "MT:Y.1X040000KA0648300";
      }

      Serial.println();
      Serial.println("========================================");
      Serial.println("Matter Commissioning Data");
      Serial.println("========================================");
      Serial.print("QR URL     : ");
      Serial.println(qrUrl);
      Serial.print("QR Payload : ");
      Serial.println(qrPayload);
      Serial.println("Manual Code: 3497-025-9332");
      Serial.println("========================================");
      Serial.println();

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

      if (mState1 != shadowMatterStates[0]) {
        shadowMatterStates[0] = mState1;
        logAndSwitch(0, mState1, "REMOTE");
        changeDetected = true;
      }

      if (mState2 != shadowMatterStates[1]) {
        shadowMatterStates[1] = mState2;
        logAndSwitch(1, mState2, "REMOTE");
        changeDetected = true;
      }

      if (mState3 != shadowMatterStates[2]) {
        shadowMatterStates[2] = mState3;
        logAndSwitch(2, mState3, "REMOTE");
        changeDetected = true;
      }

      if (mState4 != shadowMatterStates[3]) {
        shadowMatterStates[3] = mState4;
        logAndSwitch(3, mState4, "REMOTE");
        changeDetected = true;
      }

      if (changeDetected) {
        updateOLEDUI();
      }
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

    oled.clear();
    oled.println("WIPING MEMORY...");

    while (digitalRead(BTN_BUILTIN) == LOW) {
      if ((millis() - startTime) / 1000 >= 10) {
        Matter.decommission();
        delay(1000);
        NVIC_SystemReset();
        break;
      }

      digitalWrite(LEDR, !digitalRead(LEDR));
      delay(100);
    }

    digitalWrite(LEDR, HIGH);
    updateOLEDUI();
  }
}