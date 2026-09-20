#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#define WLAN_SSID      "Robotutor"
#define WLAN_PASS      "Robotutor"

#define AIO_SERVER     "io.adafruit.com"
#define AIO_SERVERPORT 1883
#define AIO_USERNAME   "smart200326"
#define AIO_KEY        "aio_NwcY48PGdQgiXiucuPCNdIbMJrBs"

#define RELAY_PIN      D0
#define FAULT_PIN      D7
#define VOLT_PIN       A0

// SoftwareSerial(rx, tx)
#define SIM800_RX      D6   // SIM800L TX -> D6
#define SIM800_TX      D5   // SIM800L RX -> D5

#define SDA_PIN        D2
#define SCL_PIN        D1

String emergencyNumber = "+919560773418";

LiquidCrystal_I2C lcd(0x27, 16, 2);

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

Adafruit_MQTT_Publish voltFeed    = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/voltage");
Adafruit_MQTT_Publish currentFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/current");
Adafruit_MQTT_Publish faultFeed   = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/fault");
Adafruit_MQTT_Publish relayFeed   = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/relay");

SoftwareSerial sim800(SIM800_RX, SIM800_TX);

bool smsSent = false;
unsigned long lastReadTime = 0;
unsigned long lastPublishTime = 0;
unsigned long lastLcdTime = 0;
int lcdPage = 0;

float lastVoltage = 0.0;
float lastCurrent = 0.0;
String lastFaultStatus = "NORMAL";
String lastRelayStatus = "OFF";

const float ADC_REF = 1.0;
const int ADC_MAX = 1023;
const float VOLTAGE_CALIBRATION = 250.0;

const float OVER_VOLTAGE_LIMIT = 260.0;
const float UNDER_VOLTAGE_LIMIT = 170.0;

void connectWiFi();
void connectMQTT();
float readVoltage();
String detectFault(float voltage, bool faultInput);
void publishAll(float voltage, float current, String faultStatus, String relayStatus);
void updateLCD();

bool sendATCommand(String cmd, String expected, unsigned long timeout);
String readSIM800Response(unsigned long timeout);
bool initSIM800L();
bool checkNetwork();
bool sendSMS(String number, String msg);
void clearSIM800Buffer();

void setup() {
  Serial.begin(9600);
  delay(1000);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(FAULT_PIN, INPUT_PULLUP);
  digitalWrite(RELAY_PIN, HIGH);

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Meter");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(2000);

  WiFi.mode(WIFI_STA);

  sim800.begin(9600);
  delay(5000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Init SIM800L");
  Serial.println("Initializing SIM800L...");

  if (!initSIM800L()) {
    Serial.println("SIM800L init failed");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("GSM Failed");
    lcd.setCursor(0, 1);
    lcd.print("Chk SIM800L");
    delay(3000);
  }

  connectWiFi();
  connectMQTT();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  delay(1000);
}

void loop() {
  connectWiFi();
  connectMQTT();

  mqtt.processPackets(100);
  mqtt.ping();

  if (millis() - lastReadTime >= 1000) {
    lastReadTime = millis();

    float voltage = readVoltage();
    float current = 0.0;

    bool externalFault = (digitalRead(FAULT_PIN) == LOW);
    String faultStatus = detectFault(voltage, externalFault);

    bool relayOn = (faultStatus == "NORMAL");
    digitalWrite(RELAY_PIN, relayOn ? LOW : HIGH);

    String relayStatus = relayOn ? "ON" : "OFF";

    if (faultStatus != "NORMAL" && !smsSent) {
      String smsText = "ALERT!\n";
      smsText += "Fault: " + faultStatus + "\n";
      smsText += "Voltage: " + String(voltage, 1) + "V\n";
      smsText += "Current: " + String(current, 1) + "A\n";
      smsText += "Relay: " + relayStatus;

      bool smsOk = sendSMS(emergencyNumber, smsText);
      if (smsOk) {
        smsSent = true;
      } else {
        Serial.println("SMS sending failed");
      }
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
    Serial.print(voltage, 2);
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

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting WiFi");
  WiFi.begin(WLAN_SSID, WLAN_PASS);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi connection failed");
  }
}

void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt.connected()) return;

  Serial.print("Connecting MQTT... ");
  int8_t ret;

  for (int i = 0; i < 3; i++) {
    ret = mqtt.connect();
    if (ret == 0) {
      Serial.println("MQTT connected");
      return;
    }
    Serial.println(mqtt.connectErrorString(ret));
    mqtt.disconnect();
    delay(2000);
  }
}

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

String detectFault(float voltage, bool faultInput) {
  if (faultInput) return "CURRENT FAULT";
  if (voltage > OVER_VOLTAGE_LIMIT) return "OVER VOLTAGE";
  if (voltage < UNDER_VOLTAGE_LIMIT) return "UNDER VOLTAGE";
  return "NORMAL";
}

void publishAll(float voltage, float current, String faultStatus, String relayStatus) {
  if (!mqtt.connected()) return;

  voltFeed.publish(voltage);
  currentFeed.publish(current);
  faultFeed.publish(faultStatus.c_str());
  relayFeed.publish(relayStatus.c_str());
}

void updateLCD() {
  lcd.clear();

  if (lcdPage == 0) {
    lcd.setCursor(0, 0);
    lcd.print("Volt:");
    lcd.print(lastVoltage, 1);
    lcd.print("V");

    lcd.setCursor(0, 1);
    lcd.print("Fault:");
    if (lastFaultStatus.length() > 10) {
      lcd.print(lastFaultStatus.substring(0, 10));
    } else {
      lcd.print(lastFaultStatus);
    }
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Relay:");
    lcd.print(lastRelayStatus);

    lcd.setCursor(0, 1);
    lcd.print("SMS:");
    lcd.print(smsSent ? "Sent" : "Wait");
  }

  lcdPage++;
  if (lcdPage > 1) lcdPage = 0;
}

void clearSIM800Buffer() {
  while (sim800.available()) sim800.read();
}

String readSIM800Response(unsigned long timeout) {
  String response = "";
  unsigned long start = millis();

  while (millis() - start < timeout) {
    while (sim800.available()) {
      char c = sim800.read();
      response += c;
    }
    delay(10);
    yield();
  }
  return response;
}

bool sendATCommand(String cmd, String expected, unsigned long timeout) {
  clearSIM800Buffer();
  sim800.println(cmd);
  Serial.print("CMD: ");
  Serial.println(cmd);

  String resp = readSIM800Response(timeout);
  Serial.print("RESP: ");
  Serial.println(resp);

  return resp.indexOf(expected) != -1;
}

bool initSIM800L() {
  for (int i = 0; i < 5; i++) {
    if (sendATCommand("AT", "OK", 3000)) {
      sendATCommand("ATE0", "OK", 2000);
      sendATCommand("AT+CMGF=1", "OK", 2000);
      sendATCommand("AT+CSCS=\"GSM\"", "OK", 2000);
      sendATCommand("AT+CNMI=1,2,0,0,0", "OK", 2000);
      sendATCommand("AT+CPMS=\"SM\",\"SM\",\"SM\"", "OK", 2000);
      checkNetwork();
      return true;
    }
    Serial.println("Retrying SIM800L...");
    delay(3000);
  }
  return false;
}

bool checkNetwork() {
  for (int i = 0; i < 8; i++) {
    clearSIM800Buffer();
    sim800.println("AT+CREG?");
    String resp = readSIM800Response(3000);

    Serial.print("Network: ");
    Serial.println(resp);

    if (resp.indexOf("+CREG: 0,1") != -1 || resp.indexOf("+CREG: 0,5") != -1 ||
        resp.indexOf("+CREG: 1,1") != -1 || resp.indexOf("+CREG: 1,5") != -1) {
      Serial.println("Network registered");
      return true;
    }

    Serial.println("Network not registered");
    delay(2000);
  }
  return false;
}

bool sendSMS(String number, String msg) {
  Serial.println("Preparing to send SMS...");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sending SMS...");

  if (!sendATCommand("AT", "OK", 3000)) {
    Serial.println("AT failed");
    return false;
  }

  if (!checkNetwork()) {
    Serial.println("No GSM network");
    return false;
  }

  if (!sendATCommand("AT+CMGF=1", "OK", 3000)) {
    Serial.println("CMGF failed");
    return false;
  }

  clearSIM800Buffer();

  sim800.print("AT+CMGS=\"");
  sim800.print(number);
  sim800.println("\"");

  unsigned long start = millis();
  String resp = "";

  while (millis() - start < 7000) {
    while (sim800.available()) {
      char c = sim800.read();
      resp += c;
    }
    if (resp.indexOf(">") != -1) break;
    delay(10);
    yield();
  }

  Serial.print("CMGS Prompt Resp: ");
  Serial.println(resp);

  if (resp.indexOf(">") == -1) {
    Serial.println("No > prompt received");
    return false;
  }

  sim800.print(msg);
  delay(500);
  sim800.write(26);

  String finalResp = readSIM800Response(15000);
  Serial.print("SMS Final Resp: ");
  Serial.println(finalResp);

  if (finalResp.indexOf("+CMGS:") != -1 || finalResp.indexOf("OK") != -1) {
    Serial.println("SMS sent successfully");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SMS Sent");
    delay(1000);
    return true;
  }

  Serial.println("SMS not sent");
  return false;
}
