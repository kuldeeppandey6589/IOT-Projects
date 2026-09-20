/***************************************************
   ESP32 Solar/Grid Auto Switching
   with Adafruit IO
 ***************************************************/

#include <WiFi.h>
#include "AdafruitIO_WiFi.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/********* Adafruit IO Config *********/
#define IO_USERNAME  "switch230526"
//Switch@123
#define IO_KEY       "aio_IVSK96NinoRUFF6zu4KQ0L3zmS45"

/********* WiFi Config *********/
#define WIFI_SSID     "homeiot"
#define WIFI_PASS     "homeiot123"

/********* Adafruit IO Setup *********/
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

/********* Feeds *********/
AdafruitIO_Feed *voltageFeed = io.feed("solar-voltage");
AdafruitIO_Feed *sourceFeed  = io.feed("power-source");

/********* LCD Setup *********/
LiquidCrystal_I2C lcd(0x27, 16, 2);

/********* Pins *********/
#define VOLTAGE_PIN   34

#define SOLAR_RELAY   25
#define GRID_RELAY_1  26
#define GRID_RELAY_2  27

/********* Relay Logic *********/
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

/********* Threshold *********/
int lowVoltageThreshold = 2000;

void setup() {

  Serial.begin(115200);

  /********* Relay Pins *********/
  pinMode(SOLAR_RELAY, OUTPUT);
  pinMode(GRID_RELAY_1, OUTPUT);
  pinMode(GRID_RELAY_2, OUTPUT);

  digitalWrite(SOLAR_RELAY, RELAY_OFF);
  digitalWrite(GRID_RELAY_1, RELAY_OFF);
  digitalWrite(GRID_RELAY_2, RELAY_OFF);

  /********* LCD *********/
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("ESP32 SolarGrid");
  lcd.setCursor(0, 1);
  lcd.print("Connecting...");
  
  /********* Connect Adafruit IO *********/
  io.connect();

  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("Connected to Adafruit IO");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Adafruit IO OK");
  delay(2000);
  lcd.clear();
}

void loop() {

  /********* Keep MQTT Connected *********/
  io.run();

  /********* Read Analog Value *********/
  int sensorValue = analogRead(VOLTAGE_PIN);

  // ESP32 ADC Reference = 3.3V
  float voltage = sensorValue * (3.3 / 4095.0);

  Serial.print("ADC: ");
  Serial.print(sensorValue);

  Serial.print(" Voltage: ");
  Serial.print(voltage);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("V:");
  lcd.print(voltage, 2);

  if(sensorValue < lowVoltageThreshold) {

    /********* GRID MODE *********/
    solarOff();
    delay(300);

    gridOn();

    Serial.println(" GRID");

    lcd.setCursor(0, 1);
    lcd.print("Source: GRID");

    /********* Send Data *********/
    sourceFeed->save("GRID");

  } else {

    /********* SOLAR MODE *********/
    gridOff();
    delay(300);

    solarOn();

    Serial.println(" SOLAR");

    lcd.setCursor(0, 1);
    lcd.print("Source: SOLAR");

    /********* Send Data *********/
    sourceFeed->save("SOLAR");
  }

  /********* Send Voltage *********/
  voltageFeed->save(voltage);

  delay(5000);
}

/********* Functions *********/

void solarOn() {
  digitalWrite(SOLAR_RELAY, RELAY_ON);
}

void solarOff() {
  digitalWrite(SOLAR_RELAY, RELAY_OFF);
}

void gridOn() {
  digitalWrite(GRID_RELAY_1, RELAY_ON);
  digitalWrite(GRID_RELAY_2, RELAY_ON);
}

void gridOff() {
  digitalWrite(GRID_RELAY_1, RELAY_OFF);
  digitalWrite(GRID_RELAY_2, RELAY_OFF);
}