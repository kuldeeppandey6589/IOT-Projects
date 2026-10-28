#include <ESP8266WiFi.h>
#include "AdafruitIO_WiFi.h"
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ==========================
// WiFi Details
// ==========================
#define WIFI_SSID   "homeiot"
#define WIFI_PASS   "homeiot123"

// ==========================
// Adafruit IO Details
// Regenerate your Adafruit IO key and paste new key here
// ==========================
#define IO_USERNAME "health160626"
#define IO_KEY      "aio_liPx42nGrtIkdNcpe9AbMNL6P35I"  

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// ==========================
// Adafruit IO Feeds
// ==========================
AdafruitIO_Feed *temperatureFeed = io.feed("temperature");
AdafruitIO_Feed *humidityFeed    = io.feed("humidity");
AdafruitIO_Feed *pulseFeed       = io.feed("pulse");
AdafruitIO_Feed *alertFeed       = io.feed("health-alert");

// ==========================
// NodeMCU Pin Setup
// ==========================
// D1 = LCD SCL
// D2 = LCD SDA
// D4 = DHT11 DATA
// D5 = Buzzer
// A0 = Heart Rate Sensor Signal

#define DHT_PIN       D4
#define DHT_TYPE      DHT11
#define PULSE_PIN     A0
#define BUZZER_PIN    D5

// ==========================
// Objects
// ==========================
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==========================
// Alert Limits
// ==========================
float highTempLimit = 38.0;
float lowTempLimit  = 30.0;

int highPulseLimit = 120;
int lowPulseLimit  = 50;

// ==========================
// Sensor Variables
// ==========================
float temperature = 0;
float humidity = 0;

int pulseRaw = 0;
int bpm = 0;

bool tempAlert = false;
bool pulseAlert = false;
bool dhtError = false;

String alertMessage = "NORMAL";

// ==========================
// Pulse Sensor Variables
// ==========================
int pulseThreshold = 550;   // adjust using Serial Monitor
bool beatDetected = false;

unsigned long lastBeatTime = 0;
unsigned long lastPulseSeen = 0;

// ==========================
// Timing
// ==========================
unsigned long lastPulseRead = 0;
const unsigned long pulseReadInterval = 20;     // pulse read every 20 ms

unsigned long lastDHTRead = 0;
const unsigned long dhtReadInterval = 2000;     // DHT read every 2 sec

unsigned long lastLCDUpdate = 0;
const unsigned long lcdUpdateInterval = 1000;

unsigned long lastAdafruitSend = 0;
const unsigned long adafruitInterval = 30000;   // send every 30 sec

// ==========================
// LCD Update
// ==========================
void updateLCD() {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print("C P:");
  lcd.print(bpm);

  lcd.setCursor(0, 1);

  if (alertMessage.length() > 16) {
    lcd.print(alertMessage.substring(0, 16));
  } else {
    lcd.print(alertMessage);
  }
}

// ==========================
// Read Heart Rate Sensor
// ==========================
void readPulseSensorFast() {
  pulseRaw = analogRead(PULSE_PIN);

  // Detect rising pulse
  if (pulseRaw > pulseThreshold && beatDetected == false) {
    beatDetected = true;

    unsigned long currentTime = millis();

    if (lastBeatTime > 0) {
      unsigned long beatInterval = currentTime - lastBeatTime;

      // valid human heart beat range
      if (beatInterval > 300 && beatInterval < 2000) {
        bpm = 60000 / beatInterval;
        lastPulseSeen = currentTime;
      }
    }

    lastBeatTime = currentTime;
  }

  // Reset beat detection when signal goes below threshold
  if (pulseRaw < pulseThreshold - 30) {
    beatDetected = false;
  }

  // If no pulse detected for 5 sec
  if (millis() - lastPulseSeen > 5000) {
    bpm = 0;
  }
}

// ==========================
// Read DHT11 Sensor
// ==========================
void readDHTSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  Serial.println();
  Serial.println("========== HEALTH DATA ==========");

  if (isnan(t) || isnan(h)) {
    dhtError = true;
    tempAlert = true;
    alertMessage = "DHT11 ERROR";

    Serial.println("DHT11 Sensor Error");
    Serial.println("Check DHT11 wiring and 10K pull-up resistor");
  } else {
    dhtError = false;
    temperature = t;
    humidity = h;

    tempAlert = false;

    if (temperature >= highTempLimit) {
      tempAlert = true;
      alertMessage = "HIGH TEMP";
    } else if (temperature <= lowTempLimit) {
      tempAlert = true;
      alertMessage = "LOW TEMP";
    }
  }

  // Pulse alert check
  pulseAlert = false;

  if (bpm == 0) {
    pulseAlert = true;
    alertMessage = "NO PULSE";
  } else if (bpm >= highPulseLimit) {
    pulseAlert = true;
    alertMessage = "HIGH PULSE";
  } else if (bpm <= lowPulseLimit) {
    pulseAlert = true;
    alertMessage = "LOW PULSE";
  }

  if (!tempAlert && !pulseAlert && !dhtError) {
    alertMessage = "NORMAL";
  }

  // Buzzer alert
  if (tempAlert || pulseAlert || dhtError) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Pulse Raw Value: ");
  Serial.println(pulseRaw);

  Serial.print("Pulse BPM: ");
  Serial.println(bpm);

  Serial.print("Alert: ");
  Serial.println(alertMessage);

  Serial.print("WiFi Status: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED");

  Serial.print("Adafruit IO Status: ");
  Serial.println(io.statusText());
}                                                                                                                                                                                                                                                                                                                                                                                                                                        

// ==========================
// Send Data to Adafruit IO
// ==========================
void sendToAdafruitIO() {
  if (io.status() < AIO_CONNECTED) {
    Serial.println("Adafruit IO not connected, data not sent");
    return;
  }

  Serial.println("Sending data to Adafruit IO...");

  temperatureFeed->save(temperature);
  delay(1000);

  humidityFeed->save(humidity);
  delay(1000);

  pulseFeed->save(bpm);
  delay(1000);

  alertFeed->save(alertMessage);
  delay(1000);

  Serial.println("Data sent to Adafruit IO");
}

// ==========================
// Connect Adafruit IO
// ==========================
void connectAdafruitIO() {
  Serial.println();
  Serial.println("Connecting to Adafruit IO...");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting");
  lcd.setCursor(0, 1);
  lcd.print("Adafruit IO");

  io.connect();

  int retry = 0;

  while (io.status() < AIO_CONNECTED && retry < 60) {
    Serial.print(".");
    Serial.print(" Status: ");
    Serial.println(io.statusText());

    io.run();
    delay(1000);
    retry++;
  }

  if (io.status() >= AIO_CONNECTED) {
    Serial.println();
    Serial.println("Adafruit IO Connected");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("AIO Connected");
    lcd.setCursor(0, 1);
    lcd.print("System Ready");

    delay(2000);
  } else {
    Serial.println();
    Serial.println("Adafruit IO Failed");
    Serial.println("Check WiFi, username, IO key, and feed names");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("AIO Failed");
    lcd.setCursor(0, 1);
    lcd.print("Check Key/WiFi");

    delay(2000);
  }
}

// ==========================
// Setup
// ==========================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Health Monitoring System Starting...");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  dht.begin();

  Wire.begin(D2, D1);   // SDA = D2, SCL = D1

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Health Monitor");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  delay(2000);

  connectAdafruitIO();
}

// ==========================
// Loop
// ==========================
void loop() {
  io.run();

  // Read pulse sensor fast
  if (millis() - lastPulseRead >= pulseReadInterval) {
    lastPulseRead = millis();
    readPulseSensorFast();
  }

  // Read DHT11 every 2 sec
  if (millis() - lastDHTRead >= dhtReadInterval) {
    lastDHTRead = millis();
    readDHTSensor();
  }

  // LCD update every 1 sec
  if (millis() - lastLCDUpdate >= lcdUpdateInterval) {
    lastLCDUpdate = millis();
    updateLCD();
  }

  // Send data to Adafruit IO every 30 sec
  if (millis() - lastAdafruitSend >= adafruitInterval) {
    lastAdafruitSend = millis();
    sendToAdafruitIO();
  }

  // Reconnect if Adafruit IO disconnected
  if (io.status() < AIO_CONNECTED) {
    static unsigned long lastReconnect = 0;

    if (millis() - lastReconnect > 15000) {
      lastReconnect = millis();
      connectAdafruitIO();
    }
  }
}