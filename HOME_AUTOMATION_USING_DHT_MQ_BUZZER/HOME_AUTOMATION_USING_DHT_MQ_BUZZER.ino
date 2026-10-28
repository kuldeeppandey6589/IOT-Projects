#include "AdafruitIO_WiFi.h"
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ==========================
// WiFi Details
// ==========================
#define WIFI_SSID   "homeiot"
#define WIFI_PASS   "homeiot123"

// ==========================
// Adafruit IO Details
// IMPORTANT: regenerate your IO key because you shared it
// ==========================
#define IO_USERNAME "home160626"
#define IO_KEY      "aio_kCOX57N0Kl9htQvHo8ZqKXO0Rxqh"

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// ==========================
// Adafruit IO Feeds
// ==========================
AdafruitIO_Feed *relay1Feed = io.feed("relay1");
AdafruitIO_Feed *relay2Feed = io.feed("relay2");
AdafruitIO_Feed *temperatureFeed = io.feed("temperature");
AdafruitIO_Feed *humidityFeed = io.feed("humidity");
AdafruitIO_Feed *gasFeed = io.feed("gas-value");
AdafruitIO_Feed *gasAlertFeed = io.feed("gas-alert");

// ==========================
// NodeMCU Pin Setup
// ==========================
// D1 = GPIO5  LCD SCL
// D2 = GPIO4  LCD SDA
// D4 = GPIO2  DHT11
// D5 = GPIO14 Buzzer
// D6 = GPIO12 Relay 1
// D7 = GPIO13 Relay 2
// A0 = MQ-2 Analog

#define DHT_PIN       D4
#define DHT_TYPE      DHT11

#define MQ2_PIN       A0
#define BUZZER_PIN    D5

#define RELAY_1       D6
#define RELAY_2       D7

// ==========================
// Objects
// ==========================
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==========================
// Settings
// ==========================
int gasLimit = 500;
float tempLimit = 40.0;

bool relay1State = false;
bool relay2State = false;

float temperature = 0;
float humidity = 0;
int gasValue = 0;
bool gasAlert = false;
bool tempAlert = false;

// Read sensor every 2 seconds
unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 2000;

// Send to Adafruit IO every 20 seconds
unsigned long lastAdafruitSend = 0;
const unsigned long adafruitSendInterval = 20000;

// ==========================
// Relay Function
// Active LOW relay module
// ==========================
void setRelay(int relayPin, bool state) {
  digitalWrite(relayPin, state ? LOW : HIGH);
}

// ==========================
// Relay 1 Callback
// ==========================
void handleRelay1(AdafruitIO_Data *data) {
  String value = data->toString();
  value.toUpperCase();

  Serial.print("Relay 1 Command: ");
  Serial.println(value);

  if (value == "ON" || value == "1" || value == "HIGH") {
    relay1State = true;
  } else {
    relay1State = false;
  }

  setRelay(RELAY_1, relay1State);
}

// ==========================
// Relay 2 Callback
// ==========================
void handleRelay2(AdafruitIO_Data *data) {
  String value = data->toString();
  value.toUpperCase();

  Serial.print("Relay 2 Command: ");
  Serial.println(value);

  if (value == "ON" || value == "1" || value == "HIGH") {
    relay2State = true;
  } else {
    relay2State = false;
  }

  setRelay(RELAY_2, relay2State);
}

// ==========================
// LCD Update
// ==========================
void updateLCD() {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print("C H:");
  lcd.print(humidity, 0);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("Gas:");
  lcd.print(gasValue);

  if (gasAlert) {
    lcd.print(" ALERT");
  } else {
    lcd.print(" OK");
  }
}

// ==========================
// Read Sensor Data
// ==========================
void readSensorsOnly() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int g = analogRead(MQ2_PIN);

  Serial.println();
  Serial.println("========== SENSOR DATA ==========");

  if (isnan(t) || isnan(h)) {
    Serial.println("DHT11 Sensor Error");
    Serial.println("Check DHT VCC, GND, DATA pin, and 10K pull-up resistor");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DHT11 Error");
    lcd.setCursor(0, 1);
    lcd.print("Check Wiring");

    return;
  }

  temperature = t;
  humidity = h;
  gasValue = g;

  gasAlert = gasValue > gasLimit;
  tempAlert = temperature > tempLimit;

  if (gasAlert || tempAlert) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("MQ-2 Gas Value: ");
  Serial.println(gasValue);

  Serial.print("Gas Alert: ");
  Serial.println(gasAlert ? "YES" : "NO");

  Serial.print("Relay 1: ");
  Serial.println(relay1State ? "ON" : "OFF");

  Serial.print("Relay 2: ");
  Serial.println(relay2State ? "ON" : "OFF");

  updateLCD();
}

// ==========================
// Send Data to Adafruit IO
// ==========================
void sendToAdafruitIO() {
  Serial.println("Sending data to Adafruit IO...");

  temperatureFeed->save(temperature);
  delay(1200);

  humidityFeed->save(humidity);
  delay(1200);

  gasFeed->save(gasValue);
  delay(1200);

  gasAlertFeed->save(gasAlert ? "GAS DETECTED" : "NORMAL");
  delay(1200);

  Serial.println("Adafruit IO data sent");
}

// ==========================
// Setup
// ==========================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(RELAY_1, OUTPUT);
  pinMode(RELAY_2, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_1, HIGH);
  digitalWrite(RELAY_2, HIGH);
  digitalWrite(BUZZER_PIN, LOW);

  dht.begin();

  Wire.begin(D2, D1);   // SDA = D2, SCL = D1

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Home Automation");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  delay(2000);

  // First sensor read before cloud connection
  readSensorsOnly();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting");
  lcd.setCursor(0, 1);
  lcd.print("Adafruit IO");

  relay1Feed->onMessage(handleRelay1);
  relay2Feed->onMessage(handleRelay2);

  Serial.print("Connecting to Adafruit IO");

  io.connect();

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("Adafruit IO Connected");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Adafruit IO");
  lcd.setCursor(0, 1);
  lcd.print("Connected");

  delay(2000);

  relay1Feed->get();
  delay(1000);
  relay2Feed->get();
  delay(1000);
}

// ==========================
// Loop
// ==========================
void loop() {
  io.run();

  if (millis() - lastSensorRead >= sensorInterval) {
    lastSensorRead = millis();
    readSensorsOnly();
  }

  if (millis() - lastAdafruitSend >= adafruitSendInterval) {
    lastAdafruitSend = millis();
    sendToAdafruitIO();
  }
}