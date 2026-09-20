/*************************************************
 ESP32 AQI Monitoring System
 ONLY MQ7 and MQ135 will activate the buzzer.
 PM2.5, temperature, and pressure will NOT activate buzzer.
*************************************************/

#include <WiFi.h>
#include "AdafruitIO_WiFi.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_BMP280.h>

// ---------- Adafruit IO ----------
#define IO_USERNAME  "AQI010526"
//aQI@123
#define IO_KEY       "aio_vaJm723Pu4ANwj3oRNTa95nBZyvr"

#define WIFI_SSID    "homeiot"
#define WIFI_PASS    "homeiot123"

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// ---------- Adafruit IO Feeds ----------
AdafruitIO_Feed *mq7Feed      = io.feed("mq7-co");
AdafruitIO_Feed *mq135Feed    = io.feed("mq135-air");
AdafruitIO_Feed *pm25Feed     = io.feed("pm25-dust");
AdafruitIO_Feed *tempFeed     = io.feed("temperature");
AdafruitIO_Feed *pressureFeed = io.feed("pressure");
AdafruitIO_Feed *aqiFeed      = io.feed("aqi-status");

// ---------- LCD ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------- BMP280 ----------
Adafruit_BMP280 bmp;

// ---------- Sensor Pins ----------
#define MQ7_PIN       34
#define MQ135_PIN     35
#define DUST_PIN      32
#define DUST_LED_PIN  25

// ---------- Buzzer Pin ----------
#define BUZZER_PIN    26

// ---------- Gas Alert Thresholds ----------
// Buzzer works ONLY for MQ7 and MQ135.
#define MQ7_ALERT_LEVEL     2000
#define MQ135_ALERT_LEVEL   2500

// ---------- Variables ----------
float mq7Value = 0;
float mq135Value = 0;
float pm25Value = 0;
float temperature = 0;
float pressure = 0;

unsigned long lastSend = 0;
const unsigned long sendInterval = 10000; // 10 seconds

// ---------- Dust Sensor Read ----------
float readDustSensor() {
  digitalWrite(DUST_LED_PIN, LOW);
  delayMicroseconds(280);

  int dustRaw = analogRead(DUST_PIN);

  delayMicroseconds(40);
  digitalWrite(DUST_LED_PIN, HIGH);
  delayMicroseconds(9680);

  float voltage = dustRaw * (3.3 / 4095.0);

  // Approx formula for GP2Y1010AU0F
  float dustDensity = (voltage - 0.6) * 500.0;

  if (dustDensity < 0) {
    dustDensity = 0;
  }

  return dustDensity;
}

// ---------- AQI Status Based on PM2.5 ----------
String getAQIStatus(float pm25) {
  if (pm25 <= 30) {
    return "Good";
  } 
  else if (pm25 <= 60) {
    return "Satisfactory";
  } 
  else if (pm25 <= 90) {
    return "Moderate";
  } 
  else if (pm25 <= 120) {
    return "Poor";
  } 
  else if (pm25 <= 250) {
    return "Very Poor";
  } 
  else {
    return "Severe";
  }
}

// ---------- Gas Alert Check ----------
// Buzzer alert is ONLY based on MQ7 and MQ135.
bool isGasAlert(float mq7, float mq135) {
  if (mq7 > MQ7_ALERT_LEVEL) {
    return true;
  }

  if (mq135 > MQ135_ALERT_LEVEL) {
    return true;
  }

  return false;
}

// ---------- Buzzer Alert ----------
void handleBuzzerAlert(bool alertState) {
  static unsigned long lastToggleTime = 0;
  static bool buzzerState = false;

  if (!alertState) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerState = false;
    return;
  }

  // Beep pattern: ON/OFF every 300 ms
  if (millis() - lastToggleTime >= 300) {
    lastToggleTime = millis();
    buzzerState = !buzzerState;

    if (buzzerState) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
}

// ---------- Smart Delay ----------
// This keeps Adafruit IO running and buzzer beeping during LCD delays.
void smartDelay(unsigned long duration, bool alertState) {
  unsigned long startTime = millis();

  while (millis() - startTime < duration) {
    io.run();
    handleBuzzerAlert(alertState);
    delay(10);
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);

  pinMode(DUST_LED_PIN, OUTPUT);
  digitalWrite(DUST_LED_PIN, HIGH);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("AQI Monitoring");
  lcd.setCursor(0, 1);
  lcd.print("System Start");
  delay(2000);
  lcd.clear();

  // BMP280 Start
  if (!bmp.begin(0x76)) {
    lcd.setCursor(0, 0);
    lcd.print("BMP280 Error");

    Serial.println("BMP280 not found!");

    // Buzzer is NOT activated here because buzzer is only for MQ7 and MQ135.
    digitalWrite(BUZZER_PIN, LOW);

    while (1);
  }

  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  io.connect();

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");
  lcd.setCursor(0, 1);
  lcd.print("Adafruit OK");
  delay(2000);
  lcd.clear();
}

// ---------- Loop ----------
void loop() {
  io.run();

  // Read sensor values
  mq7Value = analogRead(MQ7_PIN);
  mq135Value = analogRead(MQ135_PIN);
  pm25Value = readDustSensor();

  temperature = bmp.readTemperature();
  pressure = bmp.readPressure() / 100.0;

  String aqiStatus = getAQIStatus(pm25Value);

  // Buzzer alert ONLY checks MQ7 and MQ135
  bool gasAlertNow = isGasAlert(mq7Value, mq135Value);

  // Serial Monitor Output
  Serial.println("-------------");

  Serial.print("MQ7 CO: ");
  Serial.println(mq7Value);

  Serial.print("MQ135 Air: ");
  Serial.println(mq135Value);

  Serial.print("PM2.5: ");
  Serial.print(pm25Value);
  Serial.println(" ug/m3");

  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Pressure: ");
  Serial.print(pressure);
  Serial.println(" hPa");

  Serial.print("AQI: ");
  Serial.println(aqiStatus);

  Serial.print("Gas Alert: ");
  Serial.println(gasAlertNow ? "YES" : "NO");

  // ---------- LCD Display 1 ----------
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PM2.5:");
  lcd.print(pm25Value, 1);

  lcd.setCursor(0, 1);
  lcd.print("AQI:");
  lcd.print(aqiStatus);

  smartDelay(2500, gasAlertNow);

  // ---------- LCD Display 2 ----------
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MQ7:");
  lcd.print(mq7Value, 0);

  lcd.setCursor(0, 1);
  lcd.print("MQ135:");
  lcd.print(mq135Value, 0);

  smartDelay(2500, gasAlertNow);

  // ---------- LCD Display 3 ----------
  lcd.clear();

  if (gasAlertNow) {
    lcd.setCursor(0, 0);
    lcd.print("GAS ALERT!");

    lcd.setCursor(0, 1);

    if (mq7Value > MQ7_ALERT_LEVEL && mq135Value > MQ135_ALERT_LEVEL) {
      lcd.print("MQ7 & MQ135 High");
    } 
    else if (mq7Value > MQ7_ALERT_LEVEL) {
      lcd.print("MQ7 High");
    } 
    else if (mq135Value > MQ135_ALERT_LEVEL) {
      lcd.print("MQ135 High");
    }
  } 
  else {
    lcd.setCursor(0, 0);
    lcd.print("Temp:");
    lcd.print(temperature, 1);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("P:");
    lcd.print(pressure, 0);
    lcd.print("hPa");
  }

  smartDelay(2500, gasAlertNow);

  // ---------- Send Data to Adafruit IO ----------
  if (millis() - lastSend > sendInterval) {
    lastSend = millis();

    mq7Feed->save(mq7Value);
    mq135Feed->save(mq135Value);
    pm25Feed->save(pm25Value);
    tempFeed->save(temperature);
    pressureFeed->save(pressure);
    aqiFeed->save(aqiStatus);

    Serial.println("Data sent to Adafruit IO");
  }
}