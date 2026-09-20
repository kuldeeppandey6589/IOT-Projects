#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "AdafruitIO_WiFi.h"
#include "AdafruitIO_Feed.h"
#include <LiquidCrystal_I2C.h>
#include <math.h>

// =====================================================
// LCD
// =====================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================================
// WIFI + ADAFRUIT IO
// =====================================================
#define WIFI_SSID     "Robotutor"
#define WIFI_PASS     "Robotutor"
#define AIO_USERNAME  "fault250426"

//fAULT@123
#define AIO_KEY       "aio_idrC86IQG5Kw8qg7Ge96lInx8ID4"

AdafruitIO_WiFi io(
  AIO_USERNAME,
  AIO_KEY,
  WIFI_SSID,
  WIFI_PASS
);

// =====================================================
// ADAFRUIT FEEDS
// =====================================================
AdafruitIO_Feed* feed[3] = {
  io.feed("cable-fault-1"),
  io.feed("cable-fault-2"),
  io.feed("cable-fault-3")
};

// =====================================================
// SENSOR PINS
//
// RED    = GPIO 33
// YELLOW = GPIO 36
// BLUE   = GPIO 39
// =====================================================
const int sensor[3] = {33, 36, 39};

// =====================================================
// RELAY PINS
//
// Relay 1 = RED
// Relay 2 = YELLOW
// Relay 3 = BLUE
// Relay 4 = MASTER
// =====================================================
const int relay[4] = {
  25,
  26,
  27,
  32
};

#define BUZZER 14

const bool ACTIVE_LOW = true;

// =====================================================
const int T24[3] = {
  2301,
  2303,
  2303
};

const int T46[3] = {
  2837,
  2834,
  2830
};

const int T68[3] = {
  3135,
  3132,
  3118
};

const int TNF[3] = {
  3671,
  3672,
  3661
};

// =====================================================
// VARIABLES
// =====================================================
float lastSent[3] = {
  NAN,
  NAN,
  NAN
};

int lastValue[3] = {
  -1,
  -1,
  -1
};

bool timerRunning = false;
bool tripped = false;

unsigned long faultTime = 0;

// =====================================================
// RELAY CONTROL
// =====================================================
void setRelay(int n, bool on) {

  if (ACTIVE_LOW) {
    digitalWrite(
      relay[n],
      on ? LOW : HIGH
    );
  }
  else {
    digitalWrite(
      relay[n],
      on ? HIGH : LOW
    );
  }
}

// =====================================================
// NORMAL CONDITION
//
// R1 = ON
// R2 = ON
// R3 = ON
// R4 = OFF
// =====================================================
void normalRelays() {

  setRelay(0, true);
  setRelay(1, true);
  setRelay(2, true);

  setRelay(3, false);
}

// =====================================================
// TURN EVERYTHING OFF
// =====================================================
void allRelaysOff() {

  for (int i = 0; i < 4; i++) {
    setRelay(i, false);
  }
}

// =====================================================
// ADC READING
// =====================================================
int readADC(int pin) {

  long total = 0;

  for (int i = 0; i < 12; i++) {

    total += analogRead(pin);

    delay(2);
  }

  return total / 12;
}

// =====================================================
// CONVERT ADC TO FAULT DISTANCE
//
// 0 = NORMAL
// 2 = 2 KM
// 4 = 4 KM
// 6 = 6 KM
// 8 = 8 KM
// =====================================================
int getDistance(int line, int adc) {

  // Very low voltage / invalid
  if (adc < 1500) {
    return 0;
  }

  // NORMAL
  if (adc >= TNF[line]) {
    return 0;
  }

  // 8 KM
  if (adc >= T68[line]) {
    return 8;
  }

  // 6 KM
  if (adc >= T46[line]) {
    return 6;
  }

  // 4 KM
  if (adc >= T24[line]) {
    return 4;
  }

  // 2 KM
  return 2;
}

// =====================================================
// ADAFRUIT IO
// =====================================================
void sendIO(int line, int value) {

  if (
    isnan(lastSent[line]) ||
    lastSent[line] != value
  ) {

    feed[line]->save(value);

    lastSent[line] = value;
  }
}

// =====================================================
// LCD NORMAL
// =====================================================
void showNormal() {

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("SYSTEM NORMAL");

  lcd.setCursor(0, 1);
  lcd.print("NO FAULT");
}

// =====================================================
// SINGLE PHASE TO GROUND FAULT
//
// Example:
// R-G FAULT
// DIST: 4 KM
// =====================================================
void showGroundFault(
  const char* phase,
  int km
) {

  lcd.clear();

  lcd.setCursor(0, 0);

  // lcd.print(phase);
  lcd.print("L-G FAULT");

  lcd.setCursor(0, 1);

  lcd.print("DIST: ");
  lcd.print(km);
  lcd.print(" KM");
}

// =====================================================
// TWO PHASE SHORT
//
// Example:
// R-Y SHORT
// DIST: 4 KM
// =====================================================
void showPhaseShort(
  const char* phases,
  int km1,
  int km2
) {

  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print(phases);
  lcd.print(" FAULT");

  lcd.setCursor(0, 1);

  // If both phases indicate same distance
  if (km1 == km2) {

    lcd.print("DIST: ");
    lcd.print(km1);
    lcd.print(" KM");
  }

  // If readings differ slightly
  else {

    lcd.print("D:");
    lcd.print(km1);
    lcd.print("/");
    lcd.print(km2);
    lcd.print(" KM");
  }
}

// =====================================================
// THREE PHASE SHORT
// =====================================================
void showThreePhaseShort(
  int rKm,
  int yKm,
  int bKm
) {

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("3 PHASE Fault");

  lcd.setCursor(0, 1);

  // Same fault distance
  if (
    rKm == yKm &&
    yKm == bKm
  ) {

    lcd.print("DIST: ");
    lcd.print(rKm);
    lcd.print(" KM");
  }

  // Different detected distances
  else {

    lcd.print("D:");

    lcd.print(rKm);
    lcd.print("/");

    lcd.print(yKm);
    lcd.print("/");

    lcd.print(bKm);

    lcd.print("KM");
  }
}

// =====================================================
// SETUP
// =====================================================
void setup() {

  Serial.begin(115200);

  // ------------------------------
  // LCD
  // ------------------------------
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Cable Fault");

  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  // ------------------------------
  // ADC
  // ------------------------------
  analogReadResolution(12);

  for (int i = 0; i < 3; i++) {

    analogSetPinAttenuation(
      sensor[i],
      ADC_11db
    );
  }

  // ------------------------------
  // BUZZER
  // ------------------------------
  pinMode(
    BUZZER,
    OUTPUT
  );

  digitalWrite(
    BUZZER,
    LOW
  );

  // ------------------------------
  // RELAYS
  // ------------------------------
  for (int i = 0; i < 4; i++) {

    pinMode(
      relay[i],
      OUTPUT
    );

    setRelay(
      i,
      false
    );
  }

  // Normal:
  // Relay 1,2,3 ON
  // Relay 4 OFF
  normalRelays();

  // ------------------------------
  // ADAFRUIT IO
  // ------------------------------
  io.connect();

  while (io.status() < AIO_CONNECTED) {

    delay(250);

    Serial.print(".");
  }

  Serial.println();

  Serial.println(
    "Adafruit IO Connected"
  );

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Adafruit IO");

  lcd.setCursor(0, 1);
  lcd.print("Connected");

  delay(1000);

  showNormal();
}

// =====================================================
// MAIN LOOP
// =====================================================
void loop() {

  // Keep Adafruit IO connected
  io.run();

  // ===================================================
  // READ ADC
  // ===================================================
  int adc[3];

  int km[3];

  for (int i = 0; i < 3; i++) {

    adc[i] = readADC(
      sensor[i]
    );

    km[i] = getDistance(
      i,
      adc[i]
    );
  }

  // ===================================================
  // FAULT STATUS
  // ===================================================
  bool redFault =
    km[0] > 0;

  bool yellowFault =
    km[1] > 0;

  bool blueFault =
    km[2] > 0;

  bool anyFault =
    redFault ||
    yellowFault ||
    blueFault;

  // Count faulted phases
  int faultCount =
    (redFault ? 1 : 0) +
    (yellowFault ? 1 : 0) +
    (blueFault ? 1 : 0);

  // ===================================================
  // SERIAL MONITOR
  // ===================================================
  Serial.print("R=");
  Serial.print(adc[0]);

  Serial.print("  Y=");
  Serial.print(adc[1]);

  Serial.print("  B=");
  Serial.print(adc[2]);

  Serial.print("   |   R:");
  Serial.print(km[0]);

  Serial.print("KM Y:");
  Serial.print(km[1]);

  Serial.print("KM B:");
  Serial.print(km[2]);

  Serial.println("KM");

  // ===================================================
  // LCD FAULT CLASSIFICATION
  // ===================================================

  // ---------------------------------------------------
  // NO FAULT
  // ---------------------------------------------------
  if (faultCount == 0) {

    showNormal();
  }

  // ---------------------------------------------------
  // ONE PHASE FAULT
  // LINE TO GROUND
  //
  // R-G
  // Y-G
  // B-G
  // ---------------------------------------------------
  else if (faultCount == 1) {

    if (redFault) {

      showGroundFault(
        "R",
        km[0]
      );
    }

    else if (yellowFault) {

      showGroundFault(
        "Y",
        km[1]
      );
    }

    else {

      showGroundFault(
        "B",
        km[2]
      );
    }
  }

  // ---------------------------------------------------
  // TWO PHASE SHORT
  //
  // R-Y
  // Y-B
  // B-R
  // ---------------------------------------------------
  else if (faultCount == 2) {

    // R-Y SHORT
    if (
      redFault &&
      yellowFault
    ) {

      showPhaseShort(
        "Line to Line",
        km[0],
        km[1]
      );
    }

    // Y-B SHORT
    else if (
      yellowFault &&
      blueFault
    ) {

      showPhaseShort(
        "Line to Line",
        km[1],
        km[2]
      );
    }

    // B-R SHORT
    else if (
      blueFault &&
      redFault
    ) {

      showPhaseShort(
        "Line to Line",
        km[2],
        km[0]
      );
    }
  }

  // ---------------------------------------------------
  // ALL THREE PHASES FAULTED
  //
  // R-Y-B = THREE PHASE SHORT
  // ---------------------------------------------------
  else if (faultCount == 3) {

    showThreePhaseShort(
      km[0],
      km[1],
      km[2]
    );
  }

  // ===================================================
  // NEW FAULT
  //
  // Relay 1 = ON
  // Relay 2 = ON
  // Relay 3 = ON
  // Relay 4 = ON
  //
  // Buzzer = ON
  // Start 5 second timer
  // ===================================================
  if (
    anyFault &&
    !timerRunning &&
    !tripped
  ) {

    // MASTER RELAY ON
    setRelay(
      3,
      true
    );

    // BUZZER ON
    digitalWrite(
      BUZZER,
      HIGH
    );

    faultTime = millis();

    timerRunning = true;

    Serial.println(
      "FAULT DETECTED"
    );

    Serial.println(
      "MASTER RELAY ON"
    );

    Serial.println(
      "5 SECOND TIMER STARTED"
    );
  }

  // ===================================================
  // AFTER 5 SECONDS
  //
  // R1 OFF
  // R2 OFF
  // R3 OFF
  // R4 OFF
  // BUZZER OFF
  // ===================================================
  if (
    timerRunning &&
    millis() - faultTime >= 5000
  ) {

    allRelaysOff();

    digitalWrite(
      BUZZER,
      LOW
    );

    timerRunning = false;

    tripped = true;

    Serial.println(
      "5 SEC COMPLETE"
    );

    Serial.println(
      "ALL RELAYS OFF"
    );
  }

  // ===================================================
  // FAULT CLEARED
  //
  // Relay 1 ON
  // Relay 2 ON
  // Relay 3 ON
  // Relay 4 OFF
  //
  // Automatically reset
  // ===================================================
  if (!anyFault) {

    normalRelays();

    digitalWrite(
      BUZZER,
      LOW
    );

    timerRunning = false;

    tripped = false;
  }

  // ===================================================
  // SEND DISTANCE TO ADAFRUIT
  // ===================================================
  for (int i = 0; i < 3; i++) {

    if (
      lastValue[i] != km[i]
    ) {

      sendIO(
        i,
        km[i]
      );

      lastValue[i] =
        km[i];
    }
  }

  delay(100);
}