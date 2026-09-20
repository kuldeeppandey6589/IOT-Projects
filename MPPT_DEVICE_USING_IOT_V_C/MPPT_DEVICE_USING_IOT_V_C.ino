#include <WiFi.h>
#include "AdafruitIO_WiFi.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <SPI.h>
#include <SD.h>

// =======================
// Adafruit IO Details
// =======================
#define IO_USERNAME  "mqtt240426"
#define IO_KEY       "aio_Bmbz22crL8hi30wbk6pfqjxgB9mM"

#define WIFI_SSID    "homeiot"
#define WIFI_PASS    "homeiot123"

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// =======================
// LCD I2C
// =======================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =======================
// ESP32 Pins
// =======================
#define SOLAR_INPUT_VOLTAGE_PIN   34
#define SOLAR_OUTPUT_VOLTAGE_PIN  33
#define CURRENT_PIN               35
#define LDR_PIN                   32
#define DHT_PIN                   25
#define DHT_TYPE                  DHT11
#define PWM_PIN                   26
#define SD_CS_PIN                 5

DHT dht(DHT_PIN, DHT_TYPE);

// =======================
// Sensor Calibration
// =======================
float adcRef = 3.3;
float adcMax = 4095.0;

float voltageFactor = 5.0;

float acsOffset = 2.5;
float acsSensitivity = 0.185;

// =======================
// Adafruit IO Feeds
// =======================
AdafruitIO_Feed *inputVoltageFeed  = io.feed("solar-input-voltage");
AdafruitIO_Feed *outputVoltageFeed = io.feed("solar-output-voltage");
AdafruitIO_Feed *currentFeed       = io.feed("solar-current");
AdafruitIO_Feed *powerFeed         = io.feed("solar-power");
AdafruitIO_Feed *tempFeed          = io.feed("temperature");
AdafruitIO_Feed *humFeed           = io.feed("humidity");
AdafruitIO_Feed *ldrFeed           = io.feed("sunlight");
AdafruitIO_Feed *pwmFeed           = io.feed("mppt-pwm");

// =======================
// MPPT Variables
// =======================
float oldPower = 0;
float oldVoltage = 0;

int pwmValue = 120;
const int pwmFreq = 20000;
const int pwmResolution = 8;

unsigned long lastSend = 0;
const unsigned long sendInterval = 15000;

unsigned long lastSDLog = 0;
const unsigned long sdLogInterval = 5000;

bool sdReady = false;

// =======================
// Read Voltage
// =======================
float readVoltage(int pin) {
  int adc = analogRead(pin);
  float adcVoltage = adc * (adcRef / adcMax);
  float actualVoltage = adcVoltage * voltageFactor;
  return actualVoltage;
}

// =======================
// Read Current
// =======================
float readCurrent() {
  int adc = analogRead(CURRENT_PIN);
  float sensorVoltage = adc * (adcRef / adcMax);

  float current = (sensorVoltage - acsOffset) / acsSensitivity;

  if (current < 0.05 && current > -0.05) current = 0;
  if (current < 0) current = 0;

  return current;
}

// =======================
// MPPT P&O Algorithm
// =======================
void mpptControl(float inputVoltage, float power) {
  float dP = power - oldPower;
  float dV = inputVoltage - oldVoltage;

  if (dP > 0) {
    if (dV > 0) pwmValue += 2;
    else pwmValue -= 2;
  } else {
    if (dV > 0) pwmValue -= 2;
    else pwmValue += 2;
  }

  pwmValue = constrain(pwmValue, 30, 230);
  ledcWrite(PWM_PIN, pwmValue);

  oldPower = power;
  oldVoltage = inputVoltage;
}

// =======================
// Create CSV Header
// =======================
void createSDHeader() {
  if (!SD.exists("/mppt_log.csv")) {
    File file = SD.open("/mppt_log.csv", FILE_WRITE);
    if (file) {
      file.println("Time(ms),Vin(V),Vout(V),Current(A),Power(W),Temp(C),Humidity(%),Sunlight(%),PWM");
      file.close();
    }
  }
}

// =======================
// Save Data to CSV File
// =======================
void saveDataToSD(float inputVoltage, float outputVoltage, float current, float power, float temperature, float humidity, int sunlightPercent, int pwmValue) {
  if (!sdReady) return;

  File file = SD.open("/mppt_log.csv", FILE_APPEND);

  if (file) {
    file.print(millis());
    file.print(",");
    file.print(inputVoltage, 2);
    file.print(",");
    file.print(outputVoltage, 2);
    file.print(",");
    file.print(current, 2);
    file.print(",");
    file.print(power, 2);
    file.print(",");
    file.print(temperature, 1);
    file.print(",");
    file.print(humidity, 0);
    file.print(",");
    file.print(sunlightPercent);
    file.print(",");
    file.println(pwmValue);

    file.close();
    Serial.println("CSV data saved to SD card");
  } else {
    Serial.println("CSV file open failed");
  }
}

// =======================
// Setup
// =======================
void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  dht.begin();

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("ESP32 MPPT IoT");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(1000);

  ledcAttach(PWM_PIN, pwmFreq, pwmResolution);
  ledcWrite(PWM_PIN, pwmValue);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SD Checking...");

  if (SD.begin(SD_CS_PIN)) {
    sdReady = true;
    createSDHeader();

    lcd.setCursor(0, 1);
    lcd.print("SD Ready");
    Serial.println("SD card ready");
  } else {
    sdReady = false;

    lcd.setCursor(0, 1);
    lcd.print("SD Failed");
    Serial.println("SD card failed");
  }

  delay(1500);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  Serial.print("Connecting to Adafruit IO");
  io.connect();

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    lcd.setCursor(0, 1);
    lcd.print("Please wait...");
    delay(500);
  }

  Serial.println();
  Serial.println(io.statusText());

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Adafruit IO");
  lcd.setCursor(0, 1);
  lcd.print("Connected");
  delay(2000);
}

// =======================
// Loop
// =======================
void loop() {
  io.run();

  float inputVoltage = readVoltage(SOLAR_INPUT_VOLTAGE_PIN);
  float outputVoltage = readVoltage(SOLAR_OUTPUT_VOLTAGE_PIN);
  float current = inputVoltage/3.432;
  float power = outputVoltage / current;

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  int ldrRaw = analogRead(LDR_PIN);
  int sunlightPercent = map(ldrRaw, 0, 4095, 100, 0);
  sunlightPercent = constrain(sunlightPercent, 0, 100);

  if (isnan(temperature)) temperature = 0;
  if (isnan(humidity)) humidity = 0;

  mpptControl(inputVoltage, power);

  Serial.print("Input V: ");
  Serial.print(inputVoltage);
  Serial.print(" V | Output V: ");
  Serial.print(outputVoltage);
  Serial.print(" V | Current: ");
  Serial.print(current);
  Serial.print(" A | Power: ");
  Serial.print(power);
  Serial.print(" W | Temp: ");
  Serial.print(temperature);
  Serial.print(" C | Hum: ");
  Serial.print(humidity);
  Serial.print(" % | Sunlight: ");
  Serial.print(sunlightPercent);
  Serial.print(" % | PWM: ");
  Serial.println(pwmValue);

  static int screen = 0;
  static unsigned long lastScreenChange = 0;

  if (millis() - lastScreenChange > 2500) {
    lastScreenChange = millis();
    screen++;
    if (screen > 3) screen = 0;
  }

  lcd.clear();

  if (screen == 0) {
    lcd.setCursor(0, 0);
    lcd.print("Vin:");
    lcd.print(inputVoltage, 1);

    lcd.setCursor(0, 1);
    lcd.print("Vout:");
    lcd.print(outputVoltage, 1);
  }

  else if (screen == 1) {
    lcd.setCursor(0, 0);
    lcd.print("I:");
    lcd.print(current, 1);
    lcd.print("A");

    lcd.setCursor(0, 1);
    lcd.print("P:");
    lcd.print(power, 1);
    lcd.print("W");
  }

  else if (screen == 2) {
    lcd.setCursor(0, 0);
    lcd.print("Temp:");
    lcd.print(temperature, 1);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("Hum:");
    lcd.print(humidity, 0);
    lcd.print("%");
  }

  else if (screen == 3) {
    lcd.setCursor(0, 0);
    lcd.print("Sun:");
    lcd.print(sunlightPercent);
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print("PWM:");
    lcd.print(pwmValue);
  }

  if (millis() - lastSDLog >= sdLogInterval) {
    lastSDLog = millis();
    saveDataToSD(inputVoltage, outputVoltage, current, power, temperature, humidity, sunlightPercent, pwmValue);
  }

  if (millis() - lastSend >= sendInterval) {
    lastSend = millis();

    inputVoltageFeed->save(inputVoltage);
    outputVoltageFeed->save(outputVoltage);
    currentFeed->save(current);
    powerFeed->save(power);
    tempFeed->save(temperature);
    humFeed->save(humidity);
    ldrFeed->save(sunlightPercent);
    pwmFeed->save(pwmValue);

    Serial.println("Data sent to Adafruit IO");
  }

  delay(1000);
}