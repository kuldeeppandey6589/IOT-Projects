/*******************************
 ESP32 + Adafruit IO + LCD
 3 Relay Home Automation
 1 Relay for Pot Protection
********************************/

#include <WiFi.h>
#include "AdafruitIO_WiFi.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define IO_USERNAME  "home250426"
#define IO_KEY       "aio_YULU48Y02iRVpi1W444HydAns41o"

#define WIFI_SSID    "homeiot"
#define WIFI_PASS    "homeiot123"

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Relay pins
#define RELAY1 25
#define RELAY2 26
#define RELAY3 27
#define RELAY4 14

#define POT_PIN 34

#define OVER_VOLTAGE_THRESHOLD 3000
#define UNDER_VOLTAGE_THRESHOLD 800

// Active LOW relay
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// Adafruit IO feeds
AdafruitIO_Feed *relay1Feed = io.feed("relay1");
AdafruitIO_Feed *relay2Feed = io.feed("relay2");
AdafruitIO_Feed *relay3Feed = io.feed("relay3");

AdafruitIO_Feed *voltageFeed = io.feed("voltage-value");
AdafruitIO_Feed *protectionFeed = io.feed("protection-status");

bool relay1State = false;
bool relay2State = false;
bool relay3State = false;

String currentStatus = "NORMAL";
String lastProtectionStatus = "";

int lastPotValue = 0;

unsigned long lastReadTime = 0;
unsigned long lastSendTime = 0;
unsigned long lastStatusSendTime = 0;
unsigned long lastLCDTime = 0;

void setRelay(int pin, bool state) {
  digitalWrite(pin, state ? RELAY_ON : RELAY_OFF);
}

bool getStateFromFeed(AdafruitIO_Data *data) {
  String value = data->value();
  value.trim();
  value.toUpperCase();

  if (value == "1" || value == "ON" || value == "TRUE") {
    return true;
  }

  return false;
}

void printLCDLine(int row, String text) {
  lcd.setCursor(0, row);
  lcd.print(text);

  for (int i = text.length(); i < 16; i++) {
    lcd.print(" ");
  }
}

void updateLCD() {
  if (millis() - lastLCDTime < 500) return;
  lastLCDTime = millis();

  String line1 = "Volt :";
  line1 += String(lastPotValue);
  line1 += " mV:";

  String line2;

  if (currentStatus == "OVER VOLTAGE") {
    line2 = "OVER VOLT";
  } 
  else if (currentStatus == "UNDER VOLTAGE") {
    line2 = "UNDER VOLT";
  } 
  else {
    line2 = "NORMAL Volt";
  }

  printLCDLine(0, line1);
  printLCDLine(1, line2);
}

// Relay 1
void handleRelay1(AdafruitIO_Data *data) {
  relay1State = getStateFromFeed(data);
  setRelay(RELAY1, relay1State);

  Serial.print("Relay 1: ");
  Serial.println(relay1State ? "ON" : "OFF");

  updateLCD();
}

// Relay 2
void handleRelay2(AdafruitIO_Data *data) {
  relay2State = getStateFromFeed(data);
  setRelay(RELAY2, relay2State);

  Serial.print("Relay 2: ");
  Serial.println(relay2State ? "ON" : "OFF");

  updateLCD();
}

// Relay 3
void handleRelay3(AdafruitIO_Data *data) {
  relay3State = getStateFromFeed(data);
  setRelay(RELAY3, relay3State);

  Serial.print("Relay 3: ");
  Serial.println(relay3State ? "ON" : "OFF");

  updateLCD();
}

void sendProtectionStatus(String status) {
  if (status != lastProtectionStatus || millis() - lastStatusSendTime > 15000) {
    protectionFeed->save(status);
    lastProtectionStatus = status;
    lastStatusSendTime = millis();
  }
}

void checkPotProtection() {
  int potValue = analogRead(POT_PIN);
  lastPotValue = potValue;

  if (potValue > OVER_VOLTAGE_THRESHOLD) {
    currentStatus = "OVER VOLTAGE";
    setRelay(RELAY4, true);
  } 
  else if (potValue < UNDER_VOLTAGE_THRESHOLD) {
    currentStatus = "UNDER VOLTAGE";
    setRelay(RELAY4, true);
  } 
  else {
    currentStatus = "NORMAL";
    setRelay(RELAY4, false);
  }

  Serial.print("POT:");
  Serial.print(potValue);
  Serial.print(" | STATUS:");
  Serial.print(currentStatus);
  Serial.print(" | R1:");
  Serial.print(relay1State ? "ON" : "OFF");
  Serial.print(" R2:");
  Serial.print(relay2State ? "ON" : "OFF");
  Serial.print(" R3:");
  Serial.println(relay3State ? "ON" : "OFF");

  updateLCD();
  sendProtectionStatus(currentStatus);

  if (millis() - lastSendTime > 15000) {
    voltageFeed->save(potValue);
    lastSendTime = millis();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);
  delay(200);

  lcd.init();
  delay(200);
  lcd.backlight();
  lcd.clear();

  printLCDLine(0, "Home Automation");
  printLCDLine(1, "Starting...");
  delay(1000);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);
  pinMode(POT_PIN, INPUT);

  digitalWrite(RELAY1, RELAY_OFF);
  digitalWrite(RELAY2, RELAY_OFF);
  digitalWrite(RELAY3, RELAY_OFF);
  digitalWrite(RELAY4, RELAY_OFF);

  relay1Feed->onMessage(handleRelay1);
  relay2Feed->onMessage(handleRelay2);
  relay3Feed->onMessage(handleRelay3);

  printLCDLine(0, "Connecting IO");
  printLCDLine(1, "Please Wait");

  Serial.println("Connecting to Adafruit IO...");
  io.connect();

  while (io.status() < AIO_CONNECTED) {
    io.run();
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("Adafruit IO Connected!");

  printLCDLine(0, "Adafruit IO");
  printLCDLine(1, "Connected");
  delay(1000);

  relay1Feed->get();
  relay2Feed->get();
  relay3Feed->get();
}

void loop() {
  io.run();

  if (millis() - lastReadTime > 1000) {
    checkPotProtection();
    lastReadTime = millis();
  }
}