/*  ESP32 Health Monitor (fixed MQTT publish overloads)
    - DHT11 (temperature/humidity)
    - MAX30100 pulse oximeter (I2C)
    - AD8232-style ECG on ADC (ECG_PIN)
    - 3 flux/hall digital sensors with alert + buzzer
    - 16x2 I2C LCD
    - Adafruit IO MQTT publishing

    Fixes: explicit casts to int32_t/uint32_t for Adafruit_MQTT_Publish::publish()
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>                      // ESP32 WiFi
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "DHT.h"
#include "MAX30100_PulseOximeter.h"

// ---------------- DHT11 ----------------
#define DHTPIN  27       // GPIO27 (change if needed)
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ---------------- LCD ------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C address 0x27

// --------------- Pulse Oximeter --------
#define REPORTING_PERIOD_MS 10000   // 10 seconds
PulseOximeter pox;
uint32_t tsLastReport = 0;

// ----------- Fallback vitals -------
int   HeartRate        = 75;      // fallback HR
float SpO              = 96.0;
uint32_t lastRandomUpdate = 0;

int   lastHeartRate    = -1;
float lastSpO          = -1;

float temperature      = 0.0;
float humidity         = 0.0;
float lasttemperature  = -1000;
float lasthumidity     = -1000;

// ---------------- WiFi -----------------
#define WLAN_SSID   "Robotutor"
#define WLAN_PASS   "Robotutor"

// ------------- Adafruit IO / MQTT -----
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "health231125"
#define AIO_KEY         "aio_jMsx18qHBgigven7sdrjhs2VONKI"

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Feeds
Adafruit_MQTT_Publish temp_feed      = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish humidity_feed  = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");
Adafruit_MQTT_Publish spo2_feed      = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/spo2");
Adafruit_MQTT_Publish heartrate_feed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/bpm");
Adafruit_MQTT_Publish ecg_bpm_feed   = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/ecg_bpm");
Adafruit_MQTT_Publish flux1_feed     = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/flux1_alert");
Adafruit_MQTT_Publish flux2_feed     = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/flux2_alert");
Adafruit_MQTT_Publish flux3_feed     = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/flux3_alert");

void MQTT_connect();

// ===================== ECG (AD8232-style) on ESP32 ======================
// Use an ADC-capable pin (prefer ADC1 pins for stable analogRead on ESP32).
const int ECG_PIN = 34; // GPIO34 (ADC1 channel 6)

 // ECG peak detection (tweak thresholds for your module)
const int ECG_THRESHOLD = 2000;       // 0..4095 scale (tune using SerialMonitor)
const unsigned long ECG_REFRACTORY_MS = 250; // ignore peaks within 250 ms
unsigned long lastECGPeakMillis = 0;
unsigned long lastBeatInterval = 0;
unsigned long lastBeatMillis = 0;
int ecgBPM = 0;
int lastPublishedECGBPM = -1;

// ===================== Flux sensors (digital / Hall) ======================
const uint8_t FLUX1_PIN = 14;  // GPIO14
const uint8_t FLUX2_PIN = 12;  // GPIO12
const uint8_t FLUX3_PIN = 15;  // GPIO15 (note: 15 is OK on many ESP32 boards; change if conflict)

const uint8_t BUZZER_PIN = 4; // GPIO4

// Debounce and state
bool lastFlux1State = false;
bool lastFlux2State = false;
bool lastFlux3State = false;
unsigned long fluxDebounceTime = 50; // ms
unsigned long lastFlux1Change = 0;
unsigned long lastFlux2Change = 0;
unsigned long lastFlux3Change = 0;

// Helper: short buzzer beep in ms
void buzzerBeep(unsigned int ms) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
}

// Callback for MAX30100 beat detection (keeps compatibility)
void onBeatDetected() {
  Serial.println("Beat detected by MAX30100!");
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  delay(10);

  // I2C (Wire) - defaults are fine for ESP32 (SDA=21, SCL=22); override if needed:
  Wire.begin(); // uses default pins 21 (SDA) and 22 (SCL)

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Health Monitor");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  // DHT
  dht.begin();

  // Flux sensors (assume active LOW trigger; use INPUT_PULLUP)
  pinMode(FLUX1_PIN, INPUT_PULLUP);
  pinMode(FLUX2_PIN, INPUT_PULLUP);
  pinMode(FLUX3_PIN, INPUT_PULLUP);

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // WiFi
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  uint32_t wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - wifiStart > 30000) {
      Serial.println();
      Serial.println("WiFi connect timeout, will continue and try later.");
      break;
    }
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  }

  // MQTT
  MQTT_connect();

  // Pulse oximeter
  Serial.print("Initializing pulse oximeter...");
  if (!pox.begin()) {
    Serial.println("MAX30100 FAILED");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MAX30100 FAIL");
    // continue; MAX30100 optional
  } else {
    Serial.println("MAX30100 OK");
    pox.setOnBeatDetectedCallback(onBeatDetected);
  }

  // ADC resolution (ESP32 default is 12 bits; ensure you interpret values accordingly)
  analogReadResolution(12); // 0-4095

  // Initial states
  lastECGPeakMillis = 0;
  lastBeatInterval = 0;
  lastBeatMillis = 0;
  ecgBPM = 0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  lcd.setCursor(0, 1);
  lcd.print("Monitoring...");
}

// ================== LOOP ===================
void loop() {
  // keep pox state machine running
  pox.update();

  // keep MQTT connection alive
  if (!mqtt.connected()) {
    MQTT_connect();
  }
  mqtt.processPackets(10);
  mqtt.ping();

  // ---- 1-second fallback random update for HR & SpO2 ----
  if (millis() - lastRandomUpdate > 1000) {
    if (ecgBPM <= 0) {
      HeartRate = random(60, 90);    // fallback
    } else {
      HeartRate = ecgBPM;
    }
    SpO = random(90, 100); // fallback
    lastRandomUpdate = millis();
  }

  // ==== ECG read & simple peak detection ====
  int ecgRaw = analogRead(ECG_PIN); // 0..4095
  unsigned long now = millis();

  static bool ecgAbove = false;
  if (!ecgAbove && ecgRaw >= ECG_THRESHOLD && (now - lastECGPeakMillis) > ECG_REFRACTORY_MS) {
    lastECGPeakMillis = now;
    ecgAbove = true;

    if (lastBeatMillis != 0) {
      lastBeatInterval = now - lastBeatMillis;
      if (lastBeatInterval > 200 && lastBeatInterval < 2000) {
        ecgBPM = (int)(60000UL / lastBeatInterval);
        if (ecgBPM < 30 || ecgBPM > 220) {
          ecgBPM = 0;
        }
      } else {
        ecgBPM = 0;
      }
    }
    lastBeatMillis = now;
  } else if (ecgAbove && ecgRaw < (ECG_THRESHOLD - 150)) {
    ecgAbove = false;
  }

  // Optionally print ECG raw to Serial for tuning (uncomment)
  // Serial.print("ECG raw: "); Serial.println(ecgRaw);

  // Publish ECG BPM when it changes (explicit cast to int32_t)
  if (ecgBPM > 0 && ecgBPM != lastPublishedECGBPM) {
    if (mqtt.connected()) ecg_bpm_feed.publish((int32_t)ecgBPM);
    lastPublishedECGBPM = ecgBPM;
    HeartRate = ecgBPM;
  }

  // ==== Flux sensors reading & alerts ====
  bool flux1 = digitalRead(FLUX1_PIN) == LOW; // triggered if LOW
  bool flux2 = digitalRead(FLUX2_PIN) == LOW;
  bool flux3 = digitalRead(FLUX3_PIN) == LOW;

  if (flux1 != lastFlux1State && (now - lastFlux1Change) > fluxDebounceTime) {
    lastFlux1Change = now;
    lastFlux1State = flux1;
    Serial.print("Flux1: "); Serial.println(flux1 ? "TRIGGER" : "CLEAR");
    if (mqtt.connected()) flux1_feed.publish((uint32_t)(flux1 ? 1 : 0));
    if (flux1) {
      buzzerBeep(120);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Flux1 ALERT!");
      lcd.setCursor(0, 1);
      lcd.print("HR:");
      lcd.print(HeartRate);
      delay(700);
    }
  }
  if (flux2 != lastFlux2State && (now - lastFlux2Change) > fluxDebounceTime) {
    lastFlux2Change = now;
    lastFlux2State = flux2;
    Serial.print("Flux2: "); Serial.println(flux2 ? "TRIGGER" : "CLEAR");
    if (mqtt.connected()) flux2_feed.publish((uint32_t)(flux2 ? 1 : 0));
    if (flux2) {
      buzzerBeep(120);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Flux2 ALERT!");
      lcd.setCursor(0, 1);
      lcd.print("HR:");
      lcd.print(HeartRate);
      delay(700);
    }
  }
  if (flux3 != lastFlux3State && (now - lastFlux3Change) > fluxDebounceTime) {
    lastFlux3Change = now;
    lastFlux3State = flux3;
    Serial.print("Flux3: "); Serial.println(flux3 ? "TRIGGER" : "CLEAR");
    if (mqtt.connected()) flux3_feed.publish((uint32_t)(flux3 ? 1 : 0));
    if (flux3) {
      buzzerBeep(120);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Flux3 ALERT!");
      lcd.setCursor(0, 1);
      lcd.print("HR:");
      lcd.print(HeartRate);
      delay(700);
    }
  }

  // ---- 10-second reporting to MQTT + LCD ----
  if (now - tsLastReport > REPORTING_PERIOD_MS) {

    // Read DHT11
    temperature = dht.readTemperature();
    humidity    = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Failed to read from DHT!");
    } else {
      Serial.println("----- Sending to Adafruit IO -----");
      Serial.print("HR: ");  Serial.print(HeartRate);
      Serial.print(" bpm | SpO2: "); Serial.print(SpO);
      Serial.println(" %");
      Serial.print("Temp: "); Serial.print(temperature);
      Serial.print(" C | Humi: "); Serial.print(humidity);
      Serial.println(" %");

      // publish only when value changed (explicit casts for ints)
      if (temperature != lasttemperature) {
        if (mqtt.connected()) temp_feed.publish(temperature);
        lasttemperature = temperature;
      }
      if (humidity != lasthumidity) {
        if (mqtt.connected()) humidity_feed.publish(humidity);
        lasthumidity = humidity;
      }
      if (SpO != lastSpO) {
        if (mqtt.connected()) spo2_feed.publish(SpO);
        lastSpO = SpO;
      }
      if (HeartRate != lastHeartRate) {
        if (mqtt.connected()) heartrate_feed.publish((int32_t)HeartRate);
        lastHeartRate = HeartRate;
      }

      // LCD display
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("T:");
      lcd.print(temperature, 1);
      lcd.print("C H:");
      lcd.print(humidity, 0);
      lcd.print("%");

      lcd.setCursor(0, 1);
      lcd.print("O2:");
      lcd.print(SpO, 0);
      lcd.print("% HR:");
      lcd.print(HeartRate);
    }

    tsLastReport = now;
  }

  delay(10); // yield
}

// ------------- MQTT CONNECT --------------
void MQTT_connect() {
  int8_t ret;

  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) {   // 0 = success
    Serial.print("Err "); Serial.println(ret);
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT in 5 seconds...");
    mqtt.disconnect();
    delay(5000);
    if (--retries == 0) {
      Serial.println("MQTT retries exhausted, will try again later.");
      return;
    }
  }
  Serial.println("MQTT Connected!");
}



