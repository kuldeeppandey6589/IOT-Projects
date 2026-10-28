/*************************************************
   ESP8266 Health Monitoring System
   Sensors:
   1. DHT11   -> Temperature + Humidity
   2. BMP280  -> Pressure + Altitude
   3. MAX30100 -> Heart Rate + SpO2
   4. 16x2 I2C LCD
   5. Adafruit IO Cloud
 *************************************************/

#include <ESP8266WiFi.h>
#include "AdafruitIO_WiFi.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Adafruit_BMP280.h>
#include "MAX30100_PulseOximeter.h"

// =======================
// WiFi
// =======================
#define WIFI_SSID       "Robotutor"
#define WIFI_PASS       "Robotutor"

// =======================
// Adafruit IO
// =======================
#define AIO_USERNAME    "health010426"
//Health@123
#define AIO_KEY         "aio_gOss38P4CJEgHKimnhkZFYXD578X"

// =======================
// Sensor Pins
// =======================
#define DHTPIN          D4
#define DHTTYPE         DHT11

// =======================
// Objects
// =======================
AdafruitIO_WiFi io(AIO_USERNAME, AIO_KEY, WIFI_SSID, WIFI_PASS);

AdafruitIO_Feed *tempFeed     = io.feed("temperature");
AdafruitIO_Feed *humFeed      = io.feed("humidity");
AdafruitIO_Feed *pressureFeed = io.feed("pressure");
AdafruitIO_Feed *altitudeFeed = io.feed("altitude");
AdafruitIO_Feed *bpmFeed      = io.feed("bpm");
AdafruitIO_Feed *spo2Feed     = io.feed("spo2");

DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP280 bmp;
PulseOximeter pox;
LiquidCrystal_I2C lcd(0x27, 16, 2);


// =======================
// Variables
// =======================
float temperature = 0;
float humidity = 0;
float pressure = 0;
float altitude = 0;
float bpm = 0;
float spo2 = 0;

unsigned long lastUpload = 0;
unsigned long lastLCD = 0;

const unsigned long uploadInterval = 10000;   // upload every 5 sec
const unsigned long lcdInterval = 2000;      // change LCD page every 2 sec
int lcdPage = 0;

// =======================
// Beat Callback
// =======================
void onBeatDetected() {
  Serial.println("Beat detected!");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // I2C
  Wire.begin(D2, D1);   // SDA, SCL for NodeMCU

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Health Monitor");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  // DHT11
  dht.begin();

  // BMP280
  if (!bmp.begin(0x76)) {
    Serial.println("BMP280 not found at 0x76");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("BMP280 Error");
  } else {
    Serial.println("BMP280 OK");
  }

  // MAX30100
  if (!pox.begin()) {
    Serial.println("MAX30100 not found");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MAX30100 Error");
  } else {
    Serial.println("MAX30100 OK");
    pox.setOnBeatDetectedCallback(onBeatDetected);
  }

  // Adafruit IO
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("Adafruit IO Connected!");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi/AIO OK");
  lcd.setCursor(0, 1);
  lcd.print("System Ready");
  delay(2000);
}

void loop() {
  io.run();
  pox.update();

  // Read sensors
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  pressure = bmp.readPressure() / 100.0;      // hPa
  altitude = bmp.readAltitude(1013.25);       // change sea-level pressure if needed

  bpm = pox.getHeartRate();
  spo2 = pox.getSpO2();

  // Print to serial
  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.print(" C, Hum: ");
  Serial.print(humidity);
  Serial.print(" %, Pressure: ");
  Serial.print(pressure);
  Serial.print(" hPa, Altitude: ");
  Serial.print(altitude);
  Serial.print(" m, BPM: ");
  Serial.print(bpm);
  Serial.print(", SpO2: ");
  Serial.println(spo2);

  // Upload to Adafruit IO
  if (millis() - lastUpload > uploadInterval) {
    lastUpload = millis();

    if (!isnan(temperature)) tempFeed->save(temperature);
    if (!isnan(humidity)) humFeed->save(humidity);

    pressureFeed->save(pressure);
    altitudeFeed->save(altitude);

    if (bpm > 0) bpmFeed->save(bpm);
    if (spo2 > 0) spo2Feed->save(spo2);

    Serial.println("Data uploaded to Adafruit IO");
  }

  // LCD display pages
  if (millis() - lastLCD > lcdInterval) {
    lastLCD = millis();
    lcd.clear();

    if (lcdPage == 0) {
      lcd.setCursor(0, 0);
      lcd.print("Temp:");
      if (isnan(temperature)) lcd.print("Err");
      else lcd.print(temperature, 1);
      lcd.print((char)223);
      lcd.print("C");

      lcd.setCursor(0, 1);
      lcd.print("Hum:");
      if (isnan(humidity)) lcd.print("Err");
      else lcd.print(humidity, 1);
      lcd.print("%");
    }
    else if (lcdPage == 1) {
      lcd.setCursor(0, 0);
      lcd.print("Press:");
      lcd.print(pressure, 1);
      lcd.print("hPa");

      lcd.setCursor(0, 1);
      lcd.print("Alt:");
      lcd.print(altitude, 1);
      lcd.print("m");
    }
    else if (lcdPage == 2) {
      lcd.setCursor(0, 0);
      lcd.print("BPM:");
      lcd.print(bpm, 1);

      lcd.setCursor(0, 1);
      lcd.print("SpO2:");
      lcd.print(spo2, 1);
      lcd.print("%");
    }

    lcdPage++;
    if (lcdPage > 2) lcdPage = 0;
  }

  delay(100);
}