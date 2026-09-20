#include <WiFi.h>
#include "AdafruitIO_WiFi.h"
#include "Adafruit_MQTT.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ==========================
// WIFI + ADAFRUIT IO
// ==========================
#define WIFI_SSID   "Robotutor"
#define WIFI_PASS   "Robotutor"

#define AIO_USERNAME "AQI080426"
//Aqi@123
#define AIO_KEY      "aio_MKXy232hsHi48AHENyQVXt67i5r7"

AdafruitIO_WiFi io(AIO_USERNAME, AIO_KEY, WIFI_SSID, WIFI_PASS);

// ==========================
// ADAFRUIT FEEDS
// ==========================
AdafruitIO_Feed *tempFeed     = io.feed("aqi-temp");
AdafruitIO_Feed *humidityFeed = io.feed("aqi-humidity");
AdafruitIO_Feed *mq135Feed    = io.feed("aqi-mq135");
AdafruitIO_Feed *mq6Feed      = io.feed("aqi-mq6");
AdafruitIO_Feed *relay1Feed   = io.feed("aqi-relay1");
AdafruitIO_Feed *relay2Feed   = io.feed("aqi-relay2");

// ==========================
// LCD
// ==========================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==========================
// DHT11
// ==========================
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ==========================
// SENSOR PINS
// ==========================
#define MQ135_PIN 34      // MQ135 Analog Output
#define MQ6_PIN   35      // MQ6 Analog Output

// ==========================
// RELAY PINS
// ==========================
#define RELAY1_PIN 26     // Temperature Relay
#define RELAY2_PIN 27     // MQ135 Relay

// ==========================
// SETTINGS
// ==========================
#define RELAY_ACTIVE_LOW true

float tempThreshold = 32.0;     // Temperature threshold
int mq135Threshold  = 2000;     // MQ135 threshold
int mq6Threshold    = 2000;     // MQ6 threshold for gas status display

unsigned long lastSensorRead = 0;
unsigned long lastUpload = 0;
unsigned long lastLCDToggle = 0;

const unsigned long sensorInterval = 2000;
const unsigned long uploadInterval = 15000;
const unsigned long lcdInterval = 3000;

// ==========================
// GLOBAL VARIABLES
// ==========================
float temperature = 0.0;
float humidity = 0.0;
int mq135Value = 0;
int mq6Value = 0;

bool relay1State = false;
bool relay2State = false;
bool lcdPage = false;

// ==========================
// RELAY CONTROL FUNCTION
// ==========================
void setRelay(int pin, bool state) {
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(pin, state ? LOW : HIGH);
  } else {
    digitalWrite(pin, state ? HIGH : LOW);
  }
}

// ==========================
// LCD UPDATE FUNCTION
// ==========================
void updateLCDPage1() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print("C H:");
  lcd.print(humidity, 0);

  lcd.setCursor(0, 1);
  lcd.print("MQ135:");
  lcd.print(mq135Value);
}

void updateLCDPage2() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MQ6:");
  lcd.print(mq6Value);

  lcd.setCursor(0, 1);
  lcd.print("GAS:");
  if (mq6Value >= mq6Threshold) {
    lcd.print("YES");
  } else {
    lcd.print("NO ");
  }
}

void updateLCD() {
  if (lcdPage == false) {
    updateLCDPage1();
  } else {
    updateLCDPage2();
  }
  lcdPage = !lcdPage;
}

// ==========================
// ADAFRUIT IO CONNECT
// ==========================
void connectAdafruit() {
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("Adafruit IO Connected");
}

// ==========================
// SENSOR READ FUNCTION
// ==========================
void readSensors() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  mq135Value = analogRead(MQ135_PIN);
  mq6Value = analogRead(MQ6_PIN);

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT11");
    return;
  }

  // Relay 1 by temperature
  relay1State = (temperature >= tempThreshold);

  // Relay 2 by MQ135
  relay2State = (mq135Value >= mq135Threshold);

  setRelay(RELAY1_PIN, relay1State);
  setRelay(RELAY2_PIN, relay2State);

  Serial.println("========== SENSOR DATA ==========");
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("MQ135 Value: ");
  Serial.println(mq135Value);

  Serial.print("MQ6 Value: ");
  Serial.println(mq6Value);

  Serial.print("MQ6 Gas Status: ");
  Serial.println(mq6Value >= mq6Threshold ? "GAS DETECTED" : "NO GAS");

  Serial.print("Relay 1 (Temp): ");
  Serial.println(relay1State ? "ON" : "OFF");

  Serial.print("Relay 2 (MQ135): ");
  Serial.println(relay2State ? "ON" : "OFF");

  Serial.println("================================");
}

// ==========================
// ADAFRUIT IO UPLOAD
// ==========================
void uploadToAdafruit() {
  tempFeed->save(temperature);
  humidityFeed->save(humidity);
  mq135Feed->save(mq135Value);
  mq6Feed->save(mq6Value);
  relay1Feed->save(relay1State ? 1 : 0);
  relay2Feed->save(relay2State ? 1 : 0);

  Serial.println("Data uploaded to Adafruit IO");
}

// ==========================
// SETUP
// ==========================
void setup() {
  Serial.begin(115200);

  // I2C for ESP32
  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("AQI Monitoring");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(1500);

  dht.begin();

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);

  setRelay(RELAY1_PIN, false);
  setRelay(RELAY2_PIN, false);

  analogReadResolution(12);   // 0 to 4095

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");
  delay(1000);

  connectAdafruit();
  updateLCD();
}

// ==========================
// LOOP
// ==========================
void loop() {
  io.run();

  unsigned long currentMillis = millis();

  if (currentMillis - lastSensorRead >= sensorInterval) {
    lastSensorRead = currentMillis;
    readSensors();
  }

  if (currentMillis - lastUpload >= uploadInterval) {
    lastUpload = currentMillis;
    uploadToAdafruit();
  }

  if (currentMillis - lastLCDToggle >= lcdInterval) {
    lastLCDToggle = currentMillis;
    updateLCD();
  }
}