#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
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

#define SDA_PIN        D2
#define SCL_PIN        D1

#define DHTPIN         D6
#define DHTTYPE        DHT11
l
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

Adafruit_MQTT_Publish voltFeed     = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/voltage");
Adafruit_MQTT_Publish currentFeed  = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/current");
Adafruit_MQTT_Publish faultFeed    = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/fault");
Adafruit_MQTT_Publish relayFeed    = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/relay");
Adafruit_MQTT_Publish tempFeed     = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish humidityFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");

bool alertShown = false;
unsigned long lastReadTime = 0;
unsigned long lastPublishTime = 0;
unsigned long lastLcdTime = 0;
int lcdPage = 0;

float lastVoltage = 0.0;
float lastCurrent = 0.0;
float lastTemperature = 0.0;
float lastHumidity = 0.0;
String lastFaultStatus = "NORMAL";
String lastRelayStatus = "OFF";

const float ADC_REF = 1.0;
const int ADC_MAX = 1023;
const float VOLTAGE_CALIBRATION = 250.0;

const float OVER_VOLTAGE_LIMIT = 60.0;
const float UNDER_VOLTAGE_LIMIT = 40.0;

void connectWiFi();
void connectMQTT();
float readVoltage();
String detectFault(float voltage, bool faultIn78put);
void publishAll(float voltage, float current, float temperature, float humidity, String faultStatus, String relayStatus);
void updateLCD();

void setup() {
  Serial.begin(9600);
  delay(1000);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(FAULT_PIN, INPUT_PULLUP);
  digitalWrite(RELAY_PIN, HIGH);

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();

  dht.begin();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Meter");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(2000);

  WiFi.mode(WIFI_STA);

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
    float current = voltage / 8.35;

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature)) temperature = lastTemperature;
    if (isnan(humidity)) humidity = lastHumidity;

    bool externalFault = (digitalRead(FAULT_PIN) == LOW);
    String faultStatus = detectFault(voltage, externalFault);

    bool relayOn = (faultStatus == "NORMAL");
    digitalWrite(RELAY_PIN, relayOn ? LOW : HIGH);

    String relayStatus = relayOn ? "ON" : "OFF";

    if (faultStatus != "NORMAL" && !alertShown) {
      alertShown = true;
    }

    if (faultStatus == "NORMAL") {
      alertShown = false;
    }

    if (millis() - lastPublishTime >= 15000) {
      lastPublishTime = millis();
      publishAll(voltage, current, temperature, humidity, faultStatus, relayStatus);
    }

    lastVoltage = voltage;
    lastCurrent = current;
    lastTemperature = temperature;
    lastHumidity = humidity;
    lastFaultStatus = faultStatus;
    lastRelayStatus = relayStatus;

    Serial.print("Voltage: ");
    Serial.print(voltage, 2);
    Serial.print(" V | Temp: ");
    Serial.print(temperature, 1);
    Serial.print(" C | Hum: ");
    Serial.print(humidity, 1);
    Serial.print(" % | Fault: ");
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

void publishAll(float voltage, float current, float temperature, float humidity, String faultStatus, String relayStatus) {
  if (!mqtt.connected()) return;

  voltFeed.publish(voltage);
  currentFeed.publish(current);
  tempFeed.publish(temperature);
  humidityFeed.publish(humidity);
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
    lcd.print("Current:");
    lcd.print(lastVoltage / 8.35);
    lcd.print(" A");
  } 
  else if (lcdPage == 1) {
    lcd.setCursor(0, 0);
    lcd.print("Temp:");
    lcd.print(lastTemperature, 1);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("Hum:");
    lcd.print(lastHumidity, 1);
    lcd.print("%");
  } 
  else {
    lcd.setCursor(0, 0);
    lcd.print("Relay:");
    lcd.print(lastRelayStatus);

    lcd.setCursor(0, 1);
    lcd.print("Fault:");
    if (lastFaultStatus.length() > 10) {
      lcd.print(lastFaultStatus.substring(0, 10));
    } else {
      lcd.print(lastFaultStatus);
    }
  }

  lcdPage++;
  if (lcdPage > 2) lcdPage = 0;
}