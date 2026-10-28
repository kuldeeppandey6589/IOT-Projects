// #include <Arduino.h>
// #include <Wire.h>
// #include <WiFi.h>
// #include "AdafruitIO_WiFi.h"
// #include "AdafruitIO_Feed.h"
// #include <LiquidCrystal_I2C.h>

// LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address, columns, rows

// // WiFi and Adafruit IO credentials
// #define WIFI_SSID       "Robotutor"
// #define WIFI_PASS       "Robotutor"
// #define AIO_USERNAME    "fault081125"
// #define AIO_KEY         "aio_gsOA81bvIj3EWWNh1HezxccF6Pk0"

// // Adafruit IO setup
// AdafruitIO_WiFi io(AIO_USERNAME, AIO_KEY, WIFI_SSID, WIFI_PASS);
// // Feeds for three cable fault sensors
// AdafruitIO_Feed *statusFeed = io.feed("cable-fault");
// AdafruitIO_Feed *faultFeed1 = io.feed("cable-fault-1");
// AdafruitIO_Feed *faultFeed2 = io.feed("cable-fault-2");
// AdafruitIO_Feed *faultFeed3 = io.feed("cable-fault-3");

// // Analog pins for fault detection
// const int sensorPins[3] = {33, 36, 39};  // ADC1 pins (safe with WiFi)
// const int threshold = 500;
// const float resistancePerKm = 200.0;

// void setup() {
//   Serial.begin(115200);
//   delay(1000);

//   lcd.init();
//   lcd.backlight();
//   lcd.setCursor(0, 0);
//   lcd.print("Cable Fault Det.");

//   Serial.print("Connecting to Adafruit IO");
//   io.connect();
//   while(io.status() < AIO_CONNECTED) {
//     Serial.print(".");
//     Serial.println(io.statusText());
//     delay(500);
//   }
//   Serial.println("\nConnected to Adafruit IO");
// }

// void loop() {
//   for (int i = 0; i < 3; i++) {
//     int sensorValue = analogRead(sensorPins[i]);
//     Serial.print("Sensor ");
//     Serial.print(i + 1);
//     delay(1000);
//     Serial.print(" Value: ");
//     Serial.println(sensorValue);
//     delay(1000);

//     lcd.clear();
//     lcd.setCursor(0, 0);
//     lcd.print("Sensor ");
//     lcd.print(i + 1);
//     lcd.print(": ");
//     lcd.print(sensorValue);

//     if (sensorValue < threshold) {
//       float faultDistance = sensorValue / resistancePerKm;
//       lcd.setCursor(0, 1);
//       lcd.print("Fault at ");
//       lcd.print(faultDistance);
//       lcd.print("km");

//       Serial.print("Fault Detected at ");
//       Serial.print(faultDistance);
//       Serial.println(" km");

//       if (i == 0){ faultFeed1->save(faultDistance);}
//       else if (i == 1) faultFeed2->save(faultDistance);
//       else if (i == 2) faultFeed3->save(faultDistance);
//     } else {
//       lcd.setCursor(0, 1);
//       lcd.print("No Fault");

//       Serial.println("No Fault Detected");

//       if (i == 0) faultFeed1->save(0);
//       else if (i == 1) faultFeed2->save(0);
//       else if (i == 2) faultFeed3->save(0);
//     }

//     delay(1000);  // Delay between sensors 
//   }
// }














// #include <Arduino.h>
// #include <Wire.h>
// #include <WiFi.h>
// #include "AdafruitIO_WiFi.h"
// #include "AdafruitIO_Feed.h"
// #include <LiquidCrystal_I2C.h>

// LiquidCrystal_I2C lcd(0x27, 16, 2);

// // ===== WiFi / Adafruit IO (rotate your key if needed) =====
// #define WIFI_SSID "Robotutor"
// #define WIFI_PASS "Robotutor"
// #define AIO_USERNAME "fault081125"
// #define AIO_KEY "aio_gsOA81bvIj3EWWNh1HezxccF6Pk0"

// AdafruitIO_WiFi io(AIO_USERNAME, AIO_KEY, WIFI_SSID, WIFI_PASS);

// // ===== Feeds =====

// AdafruitIO_Feed *faultFeed1 = io.feed("cable-fault-1");
// AdafruitIO_Feed *faultFeed2 = io.feed("cable-fault-2");
// AdafruitIO_Feed *faultFeed3 = io.feed("cable-fault-3");
// AdafruitIO_Feed *fault = io.feed("cable-fault");

// // ===== Hardware =====
// // Use only ADC1 pins while WiFi is ON: 32–39 (OK: 33,36,39)
// const int sensorPins[3] = {33, 36, 39};

// // ===== App settings =====
// const int thresholdADC = 500;         // below => fault
// const float resistancePerKm = 200.0f; // your original formula
// const uint32_t PUBLISH_EVERY = 5000;  // ms (5 s)
// const uint32_t FEED_GAP = 150;        // ms gap between feed->save
// const float CHANGE_DELTA_KM = 0.01f;  // publish only if change > 0.01 km

// // last sent values (NaN means never)
// float lastSent[3] = {NAN, NAN, NAN};
// uint32_t lastPublish = 0;

// // ---- helpers ----
// int readADCStable(int pin)
// {
//   // average a few samples for stability
//   const int N = 8;
//   long sum = 0;
//   for (int i = 0; i < N; i++)
//   {
//     sum += analogRead(pin);
//     delay(2);
//   }
//   return (int)(sum / N);
// }

// void setup()
// {
//   Serial.begin(115200);
//   delay(200);

//   // LCD
//   lcd.init();
//   lcd.backlight();
//   lcd.setCursor(0, 0);
//   lcd.print("Cable Fault Det.");

//   // ESP32 ADC setup (12-bit default). Optional attenuation if signals >1.1V
//   // analogSetAttenuation(ADC_11db); // uncomment if needed (up to ~3.3V)

//   // Connect Adafruit IO
//   Serial.print("Connecting to Adafruit IO");
//   io.connect();

//   // Optional: faster keep-alive
//   // io.mqttClient->setKeepAlive(30);

//   while (io.status() < AIO_CONNECTED)
//   {
//     Serial.print(".");
//     delay(250);
//   }
//   Serial.println("\nConnected!");
//   lcd.setCursor(0, 1);
//   lcd.print("IO Connected     ");
// }

// void publishIfNeeded(uint8_t idx, float km)
// {
//   // send only when significantly changed or first time
//   if (isnan(lastSent[idx]) || fabs(km - lastSent[idx]) > CHANGE_DELTA_KM)
//   {
//     if (idx == 0)
//       faultFeed1->save(km);
//     else if (idx == 1)
//       faultFeed2->save(km);
//     else if (idx == 2)
//       faultFeed3->save(km);
//     lastSent[idx] = km;
//     delay(FEED_GAP);
//   }
// }

// void loop()
// {
//   // must be called frequently so connection stays healthy
//   io.run();

//   // read sensors
//   int adc[3];
//   for (int i = 0; i < 3; i++)
//   {
//     adc[i] = readADCStable(sensorPins[i]);
//   }

//   // show the last read sensor on LCD (rotate each second)
//   static uint8_t show = 0;
//   lcd.clear();
//   lcd.setCursor(0, 0);
//   lcd.print("Sensor ");
//   lcd.print(show + 1);
//   lcd.print(": ");
//   lcd.print(adc[show]);

//   // compute fault status for this sensor only for display
//   if (adc[show] < thresholdADC)
//   {
//     float km = adc[show] / resistancePerKm;
//     lcd.setCursor(0, 1);
//     lcd.print("Fault @ ");
//     lcd.print(km, 2);
//     lcd.print(" km   ");
//   }
//   else
//   {
//     lcd.setCursor(0, 1);
//     lcd.print("No Fault ");
//   }
//   show = (show + 1) % 3;

//   // publish at controlled rate (every PUBLISH_EVERY ms)
//   if (millis() - lastPublish >= PUBLISH_EVERY)
//   {
//     for (int i = 0; i < 3; i++)
//     {
//       float km = 0.0f;
//       if (adc[i] < thresholdADC)
//       {
//         km = adc[i] / 200.0f;
//         Serial.printf("S%d FAULT -> %.2f km\n", i + 1, km);
//       }
//       else
//       {
//         Serial.printf("S%d OK\n", i + 1);
//       }
//       publishIfNeeded(i, km); // 0 when OK, else distance
//     }
//     lastPublish = millis();
//   }

//   delay(1000); // small UI pacing; networking remains responsive via io.run()
// }





















// #include <Arduino.h>
// #include <Wire.h>
// #include <WiFi.h>
// #include "AdafruitIO_WiFi.h"
// #include "AdafruitIO_Feed.h"
// #include <LiquidCrystal_I2C.h>

// // -------- LCD (0x27 is common; change if your scanner shows 0x3F, etc.) -----
// LiquidCrystal_I2C lcd(0x27, 16, 2);

// // -------- WiFi / Adafruit IO creds (from your message) -----------------------
// #define WIFI_SSID    "Robotutor"
// #define WIFI_PASS    "Robotutor"
// #define AIO_USERNAME "fault081125"
// #define AIO_KEY      "aio_gsOA81bvIj3EWWNh1HezxccF6Pk0"

// // -------- Adafruit IO client -------------------------------------------------
// AdafruitIO_WiFi io(AIO_USERNAME, AIO_KEY, WIFI_SSID, WIFI_PASS);

// // Separate feeds per channel (auto-created at first publish)
// AdafruitIO_Feed *A_adc    = io.feed("A-ADC");
// AdafruitIO_Feed *A_dist   = io.feed("A-FaultDistance");
// AdafruitIO_Feed *A_fault  = io.feed("A-Fault");

// AdafruitIO_Feed *B_adc    = io.feed("B-ADC");
// AdafruitIO_Feed *B_dist   = io.feed("B-FaultDistance");
// AdafruitIO_Feed *B_fault  = io.feed("B-Fault");

// // -------- Hardware pins (ESP32 ADC) -----------------------------------------
// // Use any ADC-capable pins (keep signals 0–3.3V). 33/32 are common choices.
// const int pinA = 33;   // Channel A
// const int pinB = 32;   // Channel B

// // -------- Model / calibration ------------------------------------------------
// // Healthy ADC (no fault) vs. "near-end hard fault" ADC. Tweak to your setup.
// float CAL_NO_FAULT   = 3600.0f;  // typical healthy reading
// float CAL_FULL_FAULT = 500.0f;   // typical hard fault near end

// // Cable length used to map ratio -> distance
// float TOTAL_CABLE_LENGTH_KM = 1.0f;   // e.g., 1 km cable

// // Fault decision: if below this fraction of healthy span, call it a fault
// float FAULT_THRESHOLD_RATIO = 0.15f;  // 15%

// // Sampling / reporting
// const uint8_t  SAMPLES = 10;
// const uint32_t PUBLISH_MS = 2000;
// uint32_t lastPub = 0;

// // -------- Helpers ------------------------------------------------------------
// static uint16_t readAveraged(int pin, uint8_t n) {
//   uint32_t acc = 0;
//   for (uint8_t i = 0; i < n; i++) { acc += analogRead(pin); delay(2); }
//   return (uint16_t)(acc / n);
// }

// static float adcToDistanceKm(uint16_t adc) {
//   float span = CAL_NO_FAULT - CAL_FULL_FAULT;
//   if (span < 50.0f) span = 50.0f; // avoid tiny span issues
//   float ratio = (CAL_NO_FAULT - (float)adc) / span;  // 0 (healthy) .. 1 (max fault)
//   if (ratio < 0) ratio = 0;
//   if (ratio > 1) ratio = 1;
//   return ratio * TOTAL_CABLE_LENGTH_KM;
// }

// static bool isFault(uint16_t adc) {
//   float span = CAL_NO_FAULT - CAL_FULL_FAULT;
//   float threshold = CAL_NO_FAULT - span * FAULT_THRESHOLD_RATIO;
//   return (adc < threshold);
// }

// void setup() {
//   Serial.begin(115200);
//   delay(200);

//   // ADC setup
//   analogReadResolution(12);
//   pinMode(pinA, INPUT);
//   pinMode(pinB, INPUT);

//   // I2C + LCD (default ESP32 pins SDA=21, SCL=22; change if needed for your board)
//   Wire.begin();            // or Wire.begin(SDA, SCL);
//   lcd.init();
//   lcd.backlight();
//   lcd.clear();
//   lcd.setCursor(0,0);
//   lcd.print("Cable Fault IoT");

//   // Connect to Adafruit IO (non-blocking loop with timeout)
//   Serial.print("Connecting to Adafruit IO");
//   io.connect();
//   uint32_t t0 = millis();
//   while (io.status() < AIO_CONNECTED && millis() - t0 < 20000) {
//     Serial.print(".");
//     io.run();
//     delay(250);
//   }
//   Serial.println();
//   Serial.print("AIO status: "); Serial.println(io.statusText());
// }

// void loop() {
//   io.run(); // keep AIO client alive

//   // Read channels
//   uint16_t adcA = readAveraged(pinA, SAMPLES);
//   uint16_t adcB = readAveraged(pinB, SAMPLES);

//   float distA_km = adcToDistanceKm(adcA);
//   float distB_km = adcToDistanceKm(adcB);

//   bool faultA = isFault(adcA);
//   bool faultB = isFault(adcB);

//   // Debug
//   Serial.printf("A ADC=%u  dist=%.3f km  fault=%d |  B ADC=%u  dist=%.3f km  fault=%d\n",
//                 adcA, distA_km, faultA, adcB, distB_km, faultB);

//   // LCD (two concise lines)
//   lcd.setCursor(0,0);
//   // Example: "A:0.23km F  B:0.00km -"
//   lcd.print("A:");
//   lcd.print(distA_km, 2);
//   lcd.print("km ");
//   lcd.print(faultA ? "F " : "- ");

//   lcd.print("B:");
//   lcd.print(distB_km, 2);
//   lcd.print("km ");
//   lcd.print(faultB ? "F" : "-");

//   lcd.setCursor(0,1);
//   lcd.print("A:");
//   lcd.print(adcA);
//   lcd.print("  B:");
//   lcd.print(adcB);
//   // pad spaces to clear leftovers if values shrink
//   lcd.print("   ");

//   // Publish every PUBLISH_MS
//   uint32_t now = millis();
//   if (now - lastPub >= PUBLISH_MS) {
//     lastPub = now;

//     if (io.status() == AIO_CONNECTED) {
//       A_adc->save((int)adcA);
//       A_dist->save(distA_km);
//       A_fault->save(faultA ? 1 : 0);

//       B_adc->save((int)adcB);
//       B_dist->save(distB_km);
//       B_fault->save(faultB ? 1 : 0);

//       Serial.println("AIO publish OK");
//     } else {
//       Serial.println("AIO not connected; skipped publish");
//     }
//   }

//   delay(20);
// }


#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "AdafruitIO_WiFi.h"
#include "AdafruitIO_Feed.h"
#include <LiquidCrystal_I2C.h>
#include <math.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define WIFI_SSID "homeiot"
#define WIFI_PASS "homeiot123"
#define AIO_USERNAME "fault081125"
#define AIO_KEY "aio_gsOA81bvIj3EWWNh1HezxccF6Pk0"

AdafruitIO_WiFi io(AIO_USERNAME, AIO_KEY, WIFI_SSID, WIFI_PASS);

AdafruitIO_Feed* faultFeed1 = io.feed("cable-fault-1");
AdafruitIO_Feed* faultFeed2 = io.feed("cable-fault-2");
AdafruitIO_Feed* faultFeed3 = io.feed("cable-fault-3");

const int sensorPins[3] = {33, 36, 39};
const int relayPins[3] = {25, 26, 27};
const bool RELAY_ACTIVE_LOW = true;

// Voltage divider constants
const float VCC = 3.3;
const float SERIES_R = 1000.0; // 1k resistor
const float R_PER_KM = 200.0;  // cable resistance per km
// ADC threshold to consider "fault" (0..4095). Tune as needed.
const int thresholdADC = 500;

// If wiring = 3.3V → 1k → cable → GND (MOST common)
const bool CABLE_AFTER_R = true;

float lastSent[3] = {NAN, NAN, NAN};
uint32_t lastPublish = 0;

int readADCStable(int pin) {
  long sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  return sum / 8;
}

int computeKm(int adc, int sensorIdx) {
  // Adjust these conditions based on your calibration
  if (sensorIdx == 0) {
    if (adc > 1024 && adc < 2048) {
      return 1;
    }
    if (adc > 2048 && adc < 3072) {
      return 2;
    }
    if (adc > 3072 && adc < 4096) {
      return 3;
    }
    return -1;
  }
  if (sensorIdx == 1) {
    if (adc > 1024 && adc < 2048) {
      return 1;
    }
    if (adc > 2048 && adc < 3072) {
      return 2;
    }
    if (adc > 3072 && adc < 4096) {
      return 3;
    }
    return -1;
  }
  if (sensorIdx == 2) {
    if (adc > 1024 && adc < 2048) {
      return 1;
    }
    if (adc > 2048 && adc < 3072) {
      return 2;
    }
    if (adc > 3072 && adc < 4096) {
      return 3;
    }
    return -1;
  }
  return -1;
}

void setRelay(uint8_t idx, bool on) {
  int level = on ? (RELAY_ACTIVE_LOW ? LOW : HIGH) : (RELAY_ACTIVE_LOW ? HIGH : LOW);
  digitalWrite(relayPins[idx], level);
}

void publishIfNeeded(uint8_t idx, float km) {
  if (isnan(lastSent[idx]) || fabs(km - lastSent[idx]) > 0.01f) {
    if (idx == 0)
      faultFeed1->save(km);
    else if (idx == 1)
      faultFeed2->save(km);
    else if (idx == 2)
      faultFeed3->save(km);
    lastSent[idx] = km;
    delay(150);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Cable Fault Det.");

  // Set ADC to read full 3.3V range
  for (int i = 0; i < 3; i++) {
    pinMode(relayPins[i], OUTPUT);
    setRelay(i, false);
    analogSetPinAttenuation(sensorPins[i], ADC_11db);
  }

  Serial.print("Connecting to Adafruit IO");
  io.connect();
  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(250);
  }
  Serial.println("\nConnected!");
  lcd.setCursor(0, 1);
  lcd.print("IO Connected");
}

int lastRed = -2;
int lastYellow = -2;
int lastBlue = -2;

void loop() {
  io.run();

  int adc[3];
  for (int i = 0; i < 3; i++) {
    adc[i] = readADCStable(sensorPins[i]);
  }

  int redFault = computeKm(adc[0], 0);
  int yellowFault = computeKm(adc[1], 1);
  int blueFault = computeKm(adc[2], 2);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Y     R      G   ");
  lcd.setCursor(0, 1);
  lcd.printf(redFault > -1 ? " %dkm " : " NF  ", redFault);  
  lcd.setCursor(6, 1);
  lcd.printf(yellowFault > -1 ? " %dkm " : " NF  ", yellowFault);  
  lcd.setCursor(12, 1);
  lcd.printf(blueFault > -1 ? " %dkm " : " NF  ", blueFault);  

  digitalWrite(relayPins[0], redFault > -1);
  digitalWrite(relayPins[1], yellowFault > -1);
  digitalWrite(relayPins[2], blueFault > -1);
  
  if(lastRed != redFault) {
    publishIfNeeded(0, redFault > -1 ? (float)redFault : NAN);
    lastRed = redFault;
  }
  if(lastYellow != yellowFault) {
    publishIfNeeded(1, yellowFault > -1 ? (float)yellowFault : NAN);
    lastYellow = yellowFault;
  }
  if(lastBlue != blueFault) {
    publishIfNeeded(2, blueFault > -1 ? (float)blueFault : NAN);
    lastBlue = blueFault;
  }

  delay(1000);
}