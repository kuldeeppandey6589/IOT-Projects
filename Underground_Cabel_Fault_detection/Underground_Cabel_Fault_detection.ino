#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "AdafruitIO_WiFi.h"
#include "AdafruitIO_Feed.h"
#include <LiquidCrystal_I2C.h>
#include <math.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================
// WiFi + Adafruit IO
// =====================
#define WIFI_SSID     "Robotutor"
#define WIFI_PASS     "Robotutor"
#define AIO_USERNAME  "fault250426"
//fAULT@123
#define AIO_KEY       "aio_idrC86IQG5Kw8qg7Ge96lInx8ID4"

AdafruitIO_WiFi io(AIO_USERNAME, AIO_KEY, WIFI_SSID, WIFI_PASS);

AdafruitIO_Feed* faultFeed1 = io.feed("cable-fault-1");
AdafruitIO_Feed* faultFeed2 = io.feed("cable-fault-2");
AdafruitIO_Feed* faultFeed3 = io.feed("cable-fault-3");

// =====================
// Pins
// =====================
const int sensorPins[3] = {33, 36, 39};   // Red, Yellow, Blue ADC pins
const int relayPins[3]  = {25, 26, 27};   // Red, Yellow, Blue relay pins
const int buzzerPin     = 14;             // Buzzer pin

const bool RELAY_ACTIVE_LOW = true;

// =====================
// ADC Settings
// =====================
const int thresholdADC = 500;

float lastSent[3] = {NAN, NAN, NAN};

int lastRed = -2;
int lastYellow = -2;
int lastBlue = -2;

// =====================
// Stable ADC Reading
// =====================
int readADCStable(int pin) {
  long sum = 0;

  for (int i = 0; i < 8; i++) {
    sum += analogRead(pin);
    delay(2);
  }

  return sum / 8;
}

// =====================
// Fault Distance Calculation
// =====================
int computeKm(int adc, int sensorIdx) {
  if (adc < thresholdADC) {
    return -1;   // No fault
  }

  if (adc > 1500 && adc < 2048) {
    return 1;
  }

  if (adc > 2048 && adc < 2700) {
    return 2;
  }

  if (adc > 2700 && adc < 4000) {
    return 3;
  }

  return 0;
}

// =====================
// Relay Control
// =====================
void setRelay(uint8_t idx, bool on) {
  int level;

  if (RELAY_ACTIVE_LOW) {
    level = on ? LOW : HIGH;
  } else {
    level = on ? HIGH : LOW;
  }

  digitalWrite(relayPins[idx], level);
}

// =====================
// Buzzer Control
// =====================
void setBuzzer(bool on) {
  digitalWrite(buzzerPin, on ? HIGH : LOW);
}

// =====================
// Adafruit IO Publish
// =====================
void publishIfNeeded(uint8_t idx, float km) {
  if (isnan(lastSent[idx]) || fabs(km - lastSent[idx]) > 0.01f) {

    if (idx == 0) {
      faultFeed1->save(km);
    } 
    else if (idx == 1) {
      faultFeed2->save(km);
    } 
    else if (idx == 2) {
      faultFeed3->save(km);
    }

    lastSent[idx] = km;
    delay(150);
  }
}

// =====================
// Serial Monitor Print
// =====================
void printFaultSerial(const char* name, int adcValue, int faultKm) {
  Serial.print(name);
  Serial.print(" ADC Value: ");
  Serial.print(adcValue);
  Serial.print("  Distance: ");

  if (faultKm > -1) {
    Serial.print(faultKm);
    Serial.println(" km");
  } else {
    Serial.println("No Fault");
  }
}

// =====================
// Setup
// =====================
void setup() {
  Serial.begin(115200);
  delay(200);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Cable Fault");
  lcd.setCursor(0, 1);
  lcd.print("Detector Start");

  analogReadResolution(12);

  pinMode(buzzerPin, OUTPUT);
  setBuzzer(false);   // Buzzer OFF at start

  for (int i = 0; i < 3; i++) {
    pinMode(relayPins[i], OUTPUT);
    setRelay(i, false);

    analogSetPinAttenuation(sensorPins[i], ADC_11db);
  }

  Serial.println();
  Serial.println("Cable Fault Detector Starting...");
  Serial.print("Connecting to Adafruit IO");

  io.connect();

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(250);
  }

  Serial.println();
  Serial.println("Adafruit IO Connected!");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Adafruit IO");
  lcd.setCursor(0, 1);
  lcd.print("Connected");

  delay(1500);
}

// =====================
// Main Loop
// =====================
void loop() {
  io.run();

  int adc[3];

  for (int i = 0; i < 3; i++) {
    adc[i] = readADCStable(sensorPins[i]);
  }

  int redFault    = computeKm(adc[0], 0);
  int yellowFault = computeKm(adc[1], 1);
  int blueFault   = computeKm(adc[2], 2);

  // =====================
  // Serial Monitor
  // =====================
  Serial.println("================================");
  printFaultSerial("RED   ", adc[0], redFault);
  printFaultSerial("YELLOW", adc[1], yellowFault);
  printFaultSerial("BLUE  ", adc[2], blueFault);

  // =====================
  // LCD Display
  // =====================
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("R     Y     B");

  lcd.setCursor(0, 1);
  if (redFault > -1) {
    lcd.print(redFault);
    lcd.print("km");
  } else {
    lcd.print("NF");
  }

  lcd.setCursor(6, 1);
  if (yellowFault > -1) {
    lcd.print(yellowFault);
    lcd.print("km");
  } else {
    lcd.print("NF");
  }

  lcd.setCursor(12, 1);
  if (blueFault > -1) {
    lcd.print(blueFault);
    lcd.print("km");
  } else {
    lcd.print("NF");
  }

  // =====================
  // Relay ON when fault
  // =====================
  setRelay(0, redFault > -1);
  setRelay(1, yellowFault > -1);
  setRelay(2, blueFault > -1);

  // =====================
  // Buzzer ON when any line has fault
  // =====================
  bool anyFault = false;

  if (redFault > -1 || yellowFault > -1 || blueFault > -1) {
    anyFault = true;
  } else {
    anyFault = false;
  }

  setBuzzer(anyFault);

  // =====================
  // Send to Adafruit IO
  // No Fault = -1
  // =====================
  if (lastRed != redFault) {
    publishIfNeeded(0, redFault > -1 ? (float)redFault : -1);
    lastRed = redFault;
  }

  if (lastYellow != yellowFault) {
    publishIfNeeded(1, yellowFault > -1 ? (float)yellowFault : -1);
    lastYellow = yellowFault;
  }

  if (lastBlue != blueFault) {
    publishIfNeeded(2, blueFault > -1 ? (float)blueFault : -1);
    lastBlue = blueFault;
  }

  delay(1000);
}