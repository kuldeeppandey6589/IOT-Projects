#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// ==========================
// WiFi and Adafruit IO
// ==========================
#define WLAN_SSID      "Robotutor"
#define WLAN_PASS      "Robotutor"

#define AIO_SERVER     "io.adafruit.com"
#define AIO_SERVERPORT 1883
#define AIO_USERNAME   "smart200326"
#define AIO_KEY        "aio_NwcY48PGdQgiXiucuPCNdIbMJrBs"

// ==========================
// NodeMCU Pins
// ==========================
#define RELAY_PIN      D0
#define FAULT_PIN      D7
#define VOLT_PIN       A0

// SIM800L
#define SIM800_RX      D5   // NodeMCU RX <- SIM800L TX
#define SIM800_TX      D6   // NodeMCU TX -> SIM800L RX

// I2C LCD
#define SDA_PIN        D2
#define SCL_PIN        D1

// ==========================
// Emergency number
// ==========================
String emergencyNumber = "+918960383086";

// ==========================
// LCD
// ==========================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==========================
// Adafruit IO
// ==========================
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

Adafruit_MQTT_Publish voltFeed    = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/voltage");
Adafruit_MQTT_Publish currentFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/current");
Adafruit_MQTT_Publish faultFeed   = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/fault");
Adafruit_MQTT_Publish relayFeed   = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/relay");

// RX, TX
SoftwareSerial sim800(SIM800_RX, SIM800_TX);

// ==========================
// Variables
// ==========================
bool smsSent = false;
unsigned long lastReadTime = 0;
unsigned long lastPublishTime = 0;
unsigned long lastLcdTime = 0;
int lcdPage = 0;

float lastVoltage = 0.0;
float lastCurrent = 0.0;   // no real current on NodeMCU without extra ADC
String lastFaultStatus = "NORMAL";
String lastRelayStatus = "OFF";

// ==========================
// Voltage sensor calibration
// ==========================
// Adjust this after testing with multimeter
const float ADC_REF = 1.0;      // ESP8266 ADC reference
const int ADC_MAX = 1023;

// Example scaling factor for common voltage sensor module
const float VOLTAGE_CALIBRATION = 250.0;

// Fault thresholds
const float OVER_VOLTAGE_LIMIT = 260.0;
const float UNDER_VOLTAGE_LIMIT = 170.0;

// ==========================
// Function declarations
// ==========================
void connectWiFi();
void connectMQTT();
void sendSMS(String msg);
float readVoltage();
String detectFault(float voltage, bool faultInput);
void publishAll(float voltage, float current, String faultStatus, String relayStatus);
void updateLCD();

// ==========================
// Setup
// ==========================
void setup() {
  Serial.begin(9600);
  delay(1000);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(FAULT_PIN, INPUT_PULLUP);

  // active LOW relay: HIGH = OFF, LOW = ON
  digitalWrite(RELAY_PIN, HIGH);

  sim800.begin(9600);

  Wire.begin(SDA_PIN, SCL_PIN);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Meter");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(2000);

  connectWiFi();
  connectMQTT();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  delay(1000);

  Serial.println("System started");
}

// ==========================
// Loop
// ==========================
void loop() {
  connectWiFi();
  connectMQTT();

  mqtt.processPackets(100);
  mqtt.ping();

  if (millis() - lastReadTime >= 1000) {
    lastReadTime = millis();

    float voltage = readVoltage();

    // No real current reading possible with NodeMCU + one A0 pin
    float current = 0.0;

    bool externalFault = (digitalRead(FAULT_PIN) == LOW);
    String faultStatus = detectFault(voltage, externalFault);

    bool relayOn = true;
    if (faultStatus != "NORMAL") {
      relayOn = false;
    }

    digitalWrite(RELAY_PIN, relayOn ? LOW : HIGH);
    String relayStatus = relayOn ? "ON" : "OFF";

    if (faultStatus != "NORMAL" && !smsSent) {
      sendSMS("ALERT! Fault=" + faultStatus +
              ", Voltage=" + String(voltage, 1) +
              "V, Current=FaultInput");
      smsSent = true;
    }

    if (faultStatus == "NORMAL") {
      smsSent = false;
    }

    if (millis() - lastPublishTime >= 5000) {
      lastPublishTime = millis();
      publishAll(voltage, current, faultStatus, relayStatus);
    }

    lastVoltage = voltage;
    lastCurrent = current;
    lastFaultStatus = faultStatus;
    lastRelayStatus = relayStatus;

    Serial.print("Voltage: ");
    Serial.print(voltage);
    Serial.print(" V | Fault: ");
    Serial.print(faultStatus);
    Serial.print(" | Relay: ");
    Serial.println(relayStatus);
  }

  if (millis() - lastLcdTime >= 2000) {
    lastLcdTime = millis();
    updateLCD();
  }
}

// ==========================
// WiFi
// ==========================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  Serial.print("Connecting WiFi");
  WiFi.begin(WLAN_SSID, WLAN_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
}

// ==========================
// MQTT
// ==========================
void connectMQTT() {
  if (mqtt.connected()) return;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting MQTT");

  Serial.print("Connecting MQTT... ");
  int8_t ret;

  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    mqtt.disconnect();
    delay(5000);
  }

  Serial.println("MQTT connected");
}

// ==========================
// Read voltage from A0
// ==========================
float readVoltage() {
  long sum = 0;
  const int samples = 200;

  for (int i = 0; i < samples; i++) {
    sum += analogRead(VOLT_PIN);
    delayMicroseconds(200);
  }

  float avg = sum / (float)samples;
  float adcVoltage = (avg / ADC_MAX) * ADC_REF;
  float actualVoltage = adcVoltage * VOLTAGE_CALIBRATION;

  if (actualVoltage < 0) actualVoltage = 0;
  return actualVoltage;
}

// ==========================
// Fault logic
// ==========================
String detectFault(float voltage, bool faultInput) {
  if (faultInput) {
    return "CURRENT FAULT";
  }
  if (voltage > OVER_VOLTAGE_LIMIT) {
    return "OVER VOLTAGE";
  }
  if (voltage < UNDER_VOLTAGE_LIMIT) {
    return "UNDER VOLTAGE";
  }
  return "NORMAL";
}

// ==========================
// Publish to Adafruit IO
// ==========================
void publishAll(float voltage, float current, String faultStatus, String relayStatus) {
  voltFeed.publish(voltage);
  currentFeed.publish(current);   // placeholder 0.0
  faultFeed.publish(faultStatus.c_str());
  relayFeed.publish(relayStatus.c_str());
}

// ==========================
// SIM800L SMS
// ==========================
void sendSMS(String msg) {
  Serial.println("Sending SMS...");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sending SMS...");

  sim800.println("AT");
  delay(1000);

  sim800.println("AT+CMGF=1");
  delay(1000);

  sim800.print("AT+CMGS=\"");
  sim800.print(emergencyNumber);
  sim800.println("\"");
  delay(1000);

  sim800.print(msg);
  delay(500);

  sim800.write(26);
  delay(5000);

  Serial.println("SMS sent");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SMS Sent");
  delay(1000);
}

// ==========================
// LCD pages
// ==========================
void updateLCD() {
  lcd.clear();

  if (lcdPage == 0) {
    lcd.setCursor(0, 0);
    lcd.print("Voltage: ");
    lcd.print(lastVoltage);
    lcd.setCursor(0, 1);
    lcd.print("C: ");
    lcd.print(lastVoltage / 7.56);
    lcd.print("A");
  }
  // else if (lcdPage == 1) {
  //   lcd.setCursor(0, 0);
  //   lcd.print("Current:", "Fault Input");
  //   lcd.setCursor(0, 1);
  //   lcd.print("Fault Input");
  // }
  else if (lcdPage == 1) {
    lcd.setCursor(0, 0);
    lcd.print("Fault:");
    lcd.print(lastFaultStatus);
    lcd.setCursor(0, 1);
    lcd.print("Relay:");
    lcd.print(lastRelayStatus);
  }
  // else if (lcdPage == 3) {
  //   lcd.setCursor(0, 0);
  //   lcd.print("Relay:", lastRelayStatus);
  //   // lcd.setCursor(0, 1);
  //   // lcd.print(lastRelayStatus);
  // }

  lcdPage++;
  if (lcdPage > 1) lcdPage = 0;
}