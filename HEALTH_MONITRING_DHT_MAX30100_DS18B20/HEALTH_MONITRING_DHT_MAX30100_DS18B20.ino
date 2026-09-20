/*************************************************
   ESP8266 Health Monitoring with Pulse Sensor
   Sensors:
   - Pulse Sensor -> Heart Rate
   - DS18B20      -> Body Temperature
   - DHT11        -> Room Temperature + Humidity
   - 16x2 I2C LCD
 *************************************************/

#include <ESP8266WiFi.h>
#include "AdafruitIO_WiFi.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "DHT.h"

// ==============================
// WiFi + Adafruit IO
// ==============================
// ---------- WiFi + Adafruit IO ----------
#define IO_USERNAME  "health200426"
#define IO_KEY       "aio_zrwz15Vob0O8jJKfAs36fRZRfx0p"

#define WIFI_SSID    "homeiot"
#define WIFI_PASS    "homeiot123"

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// ==============================
// Adafruit IO Feeds
// ==============================
AdafruitIO_Feed *heartRateFeed = io.feed("health-heart-rate");
AdafruitIO_Feed *bodyTempFeed  = io.feed("health-body-temp");
AdafruitIO_Feed *roomTempFeed  = io.feed("health-room-temp");
AdafruitIO_Feed *humidityFeed  = io.feed("health-humidity");

// ==============================
// LCD
// ==============================
LiquidCrystal_I2C lcd(0x27, 16, 2);   // if not work, try 0x3F

// ==============================
// DS18B20
// ==============================
#define ONE_WIRE_BUS D5
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

// ==============================
// DHT11
// ==============================
#define DHTPIN D6
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ==============================
// Pulse Sensor
// ==============================
#define PULSE_PIN A0

int pulseValue = 0;
int threshold = 550;       // adjust after testing
bool pulseDetected = false;

unsigned long lastBeatTime = 0;
unsigned long currentBeatTime = 0;
int bpm = 0;

// ==============================
// Timers
// ==============================
unsigned long lastUploadTime = 0;
const unsigned long uploadInterval = 15000;   // 15 sec

unsigned long lastLCDTime = 0;
const unsigned long lcdInterval = 2000;

// ==============================
// Variables
// ==============================
float bodyTempC = 0.0;
float roomTempC = 0.0;
float humidity = 0.0;

byte lcdPage = 0;

// ==============================
void showPage0() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Heart:");
  lcd.print(bpm);
  lcd.print(" BPM");

  lcd.setCursor(0, 1);
  lcd.print("Body:");
  lcd.print(bodyTempC, 1);
  lcd.print((char)223);
  lcd.print("C");
}

void showPage1() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Room:");
  lcd.print(roomTempC, 1);
  lcd.print((char)223);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("Hum:");
  lcd.print(humidity, 0);
  lcd.print("%");
}

void readPulse() {
  pulseValue = analogRead(PULSE_PIN);

  // detect pulse crossing threshold
  if (pulseValue > threshold && pulseDetected == false) {
    pulseDetected = true;

    currentBeatTime = millis();
    unsigned long beatInterval = currentBeatTime - lastBeatTime;

    if (beatInterval > 300 && beatInterval < 2000) {
      bpm = 60000 / beatInterval;
      Serial.print("Beat detected! BPM: ");
      Serial.println(bpm);
    }

    lastBeatTime = currentBeatTime;
  }

  if (pulseValue < threshold) {
    pulseDetected = false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("ESP8266 Health Monitoring Start");

  // LCD I2C
  Wire.begin(D2, D1);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Health Monitor");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  // Sensors
  dht.begin();
  ds18b20.begin();

  // Connect Adafruit IO
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    io.run();
    delay(500);
  }

  Serial.println();
  Serial.println("Adafruit IO connected!");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Adafruit IO");
  lcd.setCursor(0, 1);
  lcd.print("Connected");
  delay(1500);
}

void loop() {
  io.run();

  // continuously read pulse sensor
  readPulse();

  // sensor readings
  ds18b20.requestTemperatures();
  bodyTempC = ds18b20.getTempCByIndex(0);

  roomTempC = dht.readTemperature();
  humidity = dht.readHumidity();

  // serial monitor
  Serial.print("Pulse: ");
  Serial.print(pulseValue);
  Serial.print(" | BPM: ");
  Serial.print(bpm);
  Serial.print(" | Body Temp: ");
  Serial.print(bodyTempC);
  Serial.print(" C | Room Temp: ");
  Serial.print(roomTempC);
  Serial.print(" C | Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  // LCD pages
  if (millis() - lastLCDTime >= lcdInterval) {
    lastLCDTime = millis();

    if (lcdPage == 0) {
      showPage0();
      lcdPage = 1;
    } else {
      showPage1();
      lcdPage = 0;
    }
  }

  // upload to Adafruit IO
  if (millis() - lastUploadTime >= uploadInterval) {
    lastUploadTime = millis();

    if (bpm > 30 && bpm < 180) {
      heartRateFeed->save(bpm);
      delay(500);
    }

    if (bodyTempC > -50 && bodyTempC < 125) {
      bodyTempFeed->save(bodyTempC);
      delay(500);
    }

    if (!isnan(roomTempC)) {
      roomTempFeed->save(roomTempC);
      delay(500);
    }

    if (!isnan(humidity)) {
      humidityFeed->save(humidity);
      delay(500);
    }

    Serial.println("Data uploaded to Adafruit IO");
  }

  delay(20);
}