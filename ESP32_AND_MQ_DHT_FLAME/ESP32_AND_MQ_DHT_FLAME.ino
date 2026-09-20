// #include <Arduino.h>
// #include <ESP8266WiFi.h>
// #include <SinricPro.h>
// #include <SinricProSwitch.h>

// #define WIFI_SSID     "home"
// #define WIFI_PASS     "homeiot12"

// // ==========================
// // SinricPro Credentials
// // ==========================
// #define APP_KEY       "fa4b8396-91d2-4759-86ac-d6c89e4b60a4"
// #define APP_SECRET    "efd1ddb0-147a-47dc-bbc9-f0ac245c1752-dd0fe5a2-dfd2-4087-9fe2-d1cd44434a66"

// // ==========================
// // Device IDs
// // ==========================
// #define DEVICE_ID_1   "69bffbd8dafb005af4dacacf"
// #define DEVICE_ID_2   "69bffbfbc2dbd7108b43de4d"
// #define DEVICE_ID_3   "69bffc16dafb005af4dacb2b"

// // ==========================
// // Relay Pins
// // ==========================
// #define RELAY_PIN_1   D5
// #define RELAY_PIN_2   D6
// #define RELAY_PIN_3   D7

// // Active LOW relay module
// void setRelay(int relayPin, bool state) {
//   digitalWrite(relayPin, state ? LOW : HIGH);   // ON = LOW, OFF = HIGH
// }

// bool onPowerState(const String &deviceId, bool &state) {
//   Serial.printf("Device %s turned %s\r\n", deviceId.c_str(), state ? "ON" : "OFF");

//   if (deviceId == DEVICE_ID_1) {
//     setRelay(RELAY_PIN_1, state);
//   } 
//   else if (deviceId == DEVICE_ID_2) {
//     setRelay(RELAY_PIN_2, state);
//   } 
//   else if (deviceId == DEVICE_ID_3) {
//     setRelay(RELAY_PIN_3, state);
//   }

//   return true;
// }

// void setup() {
//   Serial.begin(115200);

//   pinMode(RELAY_PIN_1, OUTPUT);
//   pinMode(RELAY_PIN_2, OUTPUT);
//   pinMode(RELAY_PIN_3, OUTPUT);

//   // All relays OFF at start
//   digitalWrite(RELAY_PIN_1, HIGH);
//   digitalWrite(RELAY_PIN_2, HIGH);
//   digitalWrite(RELAY_PIN_3, HIGH);

//   WiFi.begin(WIFI_SSID, WIFI_PASS);
//   Serial.print("Connecting to WiFi");

//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }

//   Serial.println();
//   Serial.print("Connected! IP: ");
//   Serial.println(WiFi.localIP());

//   SinricProSwitch &mySwitch1 = SinricPro[DEVICE_ID_1];
//   SinricProSwitch &mySwitch2 = SinricPro[DEVICE_ID_2];
//   SinricProSwitch &mySwitch3 = SinricPro[DEVICE_ID_3];

//   mySwitch1.onPowerState(onPowerState);
//   mySwitch2.onPowerState(onPowerState);
//   mySwitch3.onPowerState(onPowerState);

//   SinricPro.begin(APP_KEY, APP_SECRET);
//   Serial.println("SinricPro started");
// }

// void loop() {
//   SinricPro.handle();
// }


#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SinricPro.h>
#include <SinricProSwitch.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// ==========================
// WiFi
// ==========================
#define WIFI_SSID   "home"
#define WIFI_PASS   "homeiot12"

// ==========================
// SinricPro
// ==========================
#define APP_KEY       "fa4b8396-91d2-4759-86ac-d6c89e4b60a4"
#define APP_SECRET    "efd1ddb0-147a-47dc-bbc9-f0ac245c1752-dd0fe5a2-dfd2-4087-9fe2-d1cd44434a66"

#define DEVICE_ID_1   "69bffbd8dafb005af4dacacf"
#define DEVICE_ID_2   "69bffbfbc2dbd7108b43de4d"
#define DEVICE_ID_3   "69bffc16dafb005af4dacb2b"

// ==========================
// Adafruit IO
// ==========================
#define AIO_SERVER     "io.adafruit.com"
#define AIO_SERVERPORT 1883
#define AIO_USERNAME   "home220326"
#define AIO_KEY        "aio_KGjh66eazKzOAINH9rJ0yNCCIsMY"

// ==========================
// Pins
// ==========================
#define RELAY1      D5
#define RELAY2      D6
#define RELAY3      D7

#define DHTPIN      D4
#define DHTTYPE     DHT11

#define FLAME_PIN   D3
#define BUZZER_PIN  D0
#define MQ2_PIN     A0

#define SDA_PIN     D2
#define SCL_PIN     D1

// ==========================
// Objects
// ==========================
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

Adafruit_MQTT_Publish tempFeed  = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish humFeed   = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");
Adafruit_MQTT_Publish gasFeed   = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/gas");
Adafruit_MQTT_Publish flameFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/flame");

// ==========================
// Relay function
// ==========================
void setRelay(int pin, bool state) {
  digitalWrite(pin, state ? LOW : HIGH);   // active LOW relay
}

// ==========================
// Sinric callback
// ==========================
bool onPowerState(const String &deviceId, bool &state) {
  if (deviceId == DEVICE_ID_1) setRelay(RELAY1, state);
  if (deviceId == DEVICE_ID_2) setRelay(RELAY2, state);
  if (deviceId == DEVICE_ID_3) setRelay(RELAY3, state);

  Serial.printf("%s -> %s\n", deviceId.c_str(), state ? "ON" : "OFF");
  return true;
}

// ==========================
// WiFi connect
// ==========================
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");
  Serial.println(WiFi.localIP());
}

// ==========================
// MQTT connect
// ==========================
void connectMQTT() {
  if (mqtt.connected()) return;

  Serial.print("Connecting Adafruit IO");
  while (mqtt.connect() != 0) {
    Serial.print(".");
    mqtt.disconnect();
    delay(3000);
  }
  Serial.println(" connected");
}

// ==========================
// Setup
// ==========================
void setup() {
  Serial.begin(115200);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(FLAME_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
  digitalWrite(RELAY3, HIGH);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Starting...");

  dht.begin();

  connectWiFi();
  connectMQTT();

  SinricProSwitch &sw1 = SinricPro[DEVICE_ID_1];
  SinricProSwitch &sw2 = SinricPro[DEVICE_ID_2];
  SinricProSwitch &sw3 = SinricPro[DEVICE_ID_3];

  sw1.onPowerState(onPowerState);
  sw2.onPowerState(onPowerState);
  sw3.onPowerState(onPowerState);

  SinricPro.begin(APP_KEY, APP_SECRET);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  delay(1000);
}

// ==========================
// Loop
// ==========================
void loop() {
  SinricPro.handle();

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  connectMQTT();
  mqtt.processPackets(10);
  mqtt.ping();

  static unsigned long lastRead = 0;
  if (millis() - lastRead > 10000) {
    lastRead = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int gasValue = analogRead(MQ2_PIN);
    int flameValue = digitalRead(FLAME_PIN);   // LOW = flame detected on most modules

    bool alarm = false;
    if (gasValue > 350) alarm = true;
    if (flameValue == LOW) alarm = true;
    if (!isnan(t) && t > 35) alarm = true;
    if (!isnan(h) && h > 60) alarm = true;

    if (alarm) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }

    Serial.println("------ DATA ------");
    Serial.print("Temp: "); Serial.println(t);
    Serial.print("Hum : "); Serial.println(h);
    Serial.print("Gas : "); Serial.println(gasValue);
    Serial.print("Flame: "); Serial.println(flameValue == LOW ? "YES" : "NO");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T:");
    if (isnan(t)) lcd.print("Err");
    else lcd.print((int)t);

    lcd.print(" H:");
    if (isnan(h)) lcd.print("Err");
    else lcd.print((int)h);

    lcd.setCursor(0, 1);
    lcd.print("G:");
    lcd.print(gasValue);
    lcd.print(" F:");
    lcd.print(flameValue == LOW ? "Y" : "N");

    if (!isnan(t)) tempFeed.publish(t);
    if (!isnan(h)) humFeed.publish(h);
    gasFeed.publish((int32_t)gasValue);
    flameFeed.publish(flameValue == LOW ? "YES" : "NO");
  }
},