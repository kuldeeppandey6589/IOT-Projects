// // /***************************************************
// //    IoT Temperature & Humidity Monitoring + 2-CH Relay + LCD
// //    Board: ESP8266 (NodeMCU / Wemos D1 Mini)
// //    Sensor: DHT11 / DHT22
// //    LCD: 16x2 I2C
// //    MQTT: Adafruit IO
// // ***************************************************/

// #include <ESP8266WiFi.h>
// #include "Adafruit_MQTT.h"
// #include "Adafruit_MQTT_Client.h"
// #include "DHT.h"
// #include <Wire.h>
// #include <LiquidCrystal_I2C.h>

// // ----------- USER SETTINGS --------------
// #define WIFI_SSID "Robotutor"
// #define WIFI_PASS "Robotutor"

// #define AIO_USERNAME "temp161125"
// // PASSWORD : Temp@123
// #define AIO_KEY "aio_IRLi90hZdRyJ2qPBpgxSNH1hACqJ"
// // ----------------------------------------

// // MQTT
// WiFiClient client;
// Adafruit_MQTT_Client mqtt(&client,
//                           "io.adafruit.com",
//                           1883,
//                           AIO_USERNAME,
//                           AIO_KEY);

// // Feeds
// Adafruit_MQTT_Publish temperatureFeed =
//     Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");

// Adafruit_MQTT_Publish humidityFeed =
//     Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");

// // DHT
// #define DHTPIN D3
// #define DHTTYPE DHT11
// DHT dht(DHTPIN, DHTTYPE);

// // Relay pins
// #define RELAY1 D5
// #define RELAY2 D4

// // LCD
// LiquidCrystal_I2C lcd(0x27, 16, 2);  // Change 0x27 to 0x3F if needed

// // -------- RANGE SETTINGS --------
// float TEMP_HIGH = 35.0;
// float TEMP_LOW  = 24.0;

// float HUM_HIGH  = 75.0;
// float HUM_LOW   = 50.0;
// //---------------------------------

// // -------- PUBLISH RATE (avoid throttle) --------
// const unsigned long PUBLISH_INTERVAL_MS = 15000;   // 15 sec -> 2 feeds = 8 points/min
// unsigned long lastPublish = 0;
// //---------------------------------

// void connectToMQTT()
// {
//   while (!mqtt.connected())
//   {
//     Serial.print("Connecting to MQTT...");
//     if (mqtt.connect())
//     {
//       Serial.println("Connected!");
//     }
//     else
//     {
//       Serial.println("Failed, retrying in 2s...");
//       delay(2000);
//     }
//   }
// }

// void setup()
// {
//   Serial.begin(115200);

//   // LCD
//   lcd.init();
//   lcd.backlight();
//   lcd.clear();
//   lcd.setCursor(0, 0);
//   lcd.print("IoT System Boot");
//   lcd.setCursor(0, 1);
//   lcd.print("Connecting WiFi");

//   dht.begin();
//   pinMode(RELAY1, OUTPUT);
//   pinMode(RELAY2, OUTPUT);

//   // Relays off (depends on your module; HIGH usually = OFF)
//   digitalWrite(RELAY1, LOW);
//   digitalWrite(RELAY2, LOW);

//   // WiFi
//   Serial.print("Connecting to WiFi");
//   WiFi.begin(WIFI_SSID, WIFI_PASS);
//   while (WiFi.status() != WL_CONNECTED)
//   {
//     Serial.print(".");
//     delay(300);
//   }
//   Serial.println(" Connected!");

//   lcd.clear();
//   lcd.setCursor(0, 0);
//   lcd.print("WiFi Connected");
//   delay(1000);

//   connectToMQTT();
// }

// void loop()
// {
//   // Keep MQTT connected
//   if (!mqtt.connected())
//   {
//     connectToMQTT();
//   }

//   mqtt.processPackets(10);
//   mqtt.ping();   // keep connection alive

//   // Only read sensor & publish every PUBLISH_INTERVAL_MS
//   unsigned long now = millis();
//   if (now - lastPublish < PUBLISH_INTERVAL_MS)
//   {
//     return;  // too soon, wait
//   }
//   lastPublish = now;

//   float humidity = dht.readHumidity();
//   float temperature = dht.readTemperature();

//   if (isnan(humidity) || isnan(temperature))
//   {
//     Serial.println("DHT reading failed!");
//     lcd.clear();
//     lcd.setCursor(0, 0);
//     lcd.print("Sensor Error!");
//     return;
//   }

//   // Serial Output
//   Serial.print("Temp: ");
//   Serial.print(temperature);
//   Serial.print(" °C | Humidity: ");
//   Serial.print(humidity);
//   Serial.println(" %");

//   // LCD Output
//   lcd.clear();
//   lcd.setCursor(0, 0);
//   lcd.print("Temp: ");
//   lcd.print(temperature, 1);
//   lcd.print(" C ");

//   lcd.setCursor(0, 1);
//   lcd.print("Hum: ");
//   lcd.print(humidity, 1);
//   lcd.print(" % ");

//   // Publish to Adafruit IO
//   if (!temperatureFeed.publish(temperature))
//     Serial.println("Temp publish failed");
//   if (!humidityFeed.publish(humidity))
//     Serial.println("Hum publish failed");

//   // Relay 1: Temperature
//   if (temperature >= TEMP_HIGH)
//   {
//     digitalWrite(RELAY1, HIGH);  // ON
//     Serial.println("Relay 1 ON (High Temp)");
//   }
//   else if (temperature <= TEMP_LOW)
//   {
//     digitalWrite(RELAY1, HIGH); // OFF
//     Serial.println("Relay 1 OFF (Low Temp)");
//   }

//   // Relay 2: Humidity
//   if (humidity >= HUM_HIGH)
//   {
//     digitalWrite(RELAY2, HIGH);  // ON
//     Serial.println("Relay 2 ON (High Humidity)");
//   }
//   else if (humidity <= HUM_LOW)
//   {
//     digitalWrite(RELAY2, HIGH); // OFF
//     Serial.println("Relay 2 OFF (Low Humidity)");
//   }
// }

// ------------------ RELAY LOGIC FIXED VERSION ------------------
/***************************************************
   IoT Temperature & Humidity Monitoring + 2-CH Relay + LCD
   Board: ESP8266 (NodeMCU)
   Sensor: DHT11 / DHT22
   LCD: 16x2 I2C
   MQTT: Adafruit IO
   Added: Manual Relay ON/OFF from Adafruit IO
***************************************************/

#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "DHT.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ----------- USER SETTINGS --------------
#define WIFI_SSID "Robotutor"
#define WIFI_PASS "Robotutor"

#define AIO_USERNAME "temp161125"
#define AIO_KEY "aio_IRLi90hZdRyJ2qPBpgxSNH1hACqJ"
// ----------------------------------------

// MQTT Setup
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, "io.adafruit.com", 1883, AIO_USERNAME, AIO_KEY);

// Sensor Feeds
Adafruit_MQTT_Publish temperatureFeed =
    Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");

Adafruit_MQTT_Publish humidityFeed =
    Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");

// Relay Control Feeds (Manual)
// Adafruit_MQTT_Subscribe relay1Feed =
//     Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/relay1");

// Adafruit_MQTT_Subscribe relay2Feed =
//     Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/relay2");

Adafruit_MQTT_Subscribe Light1 = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/relay1");
Adafruit_MQTT_Subscribe Light2 = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/relay2");

// DHT
#define DHTPIN D3
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Relay pins (Active LOW relays)
#define RELAY1 D5
#define RELAY2 D4

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// -------- RANGE SETTINGS --------
float TEMP_HIGH = 32.0;
float TEMP_LOW = 29.0;

float HUM_HIGH = 45.0;
float HUM_LOW = 35.0;
//---------------------------------

// Auto/manual control flags
String relay1Mode = "AUTO";  // AUTO / ON / OFF
String relay2Mode = "AUTO";

// Publish interval
const unsigned long PUBLISH_INTERVAL_MS = 15000;
unsigned long lastPublish = 0;

// MQTT Connect
void connectToMQTT()
{
    Serial.print("Connecting to MQTT...");
    if (mqtt.connect())
      Serial.println("Connected!");
    else
    {
      Serial.println("Failed, retrying...");
      delay(2000);
    }
  }


void setup()
{
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Booting System");

  dht.begin();

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);

  // RELAYS OFF (Active LOW)
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(200);
  }
  Serial.println("WiFi Connected");

  lcd.clear();
  lcd.print("WiFi Connected");

  // MQTT subscriptions
  // mqtt.subscribe(&relay1Feed);
  // mqtt.subscribe(&relay2Feed);

  mqtt.subscribe(&Light1);
  mqtt.subscribe(&Light2);
  connectToMQTT();
}

void loop()
{
  if (!mqtt.connected())
    connectToMQTT();

  mqtt.processPackets(10);
  mqtt.ping();

  // ----------- READ MANUAL COMMANDS -----------
  Adafruit_MQTT_Subscribe *sub;
  while ((sub = mqtt.readSubscription(10)))
  {
    if (sub == &Light1)
    {
      relay1Mode = (char *)Light1.lastread;
      Serial.print("Relay1 Command: ");
      Serial.println(relay1Mode);
    }

    if (sub == &Light2)
    {
      relay2Mode = (char *)Light2.lastread;
      Serial.print("Relay2 Command: ");
      Serial.println(relay2Mode);
    }
  }
  // --------------------------------------------

  unsigned long now = millis();
  if (now - lastPublish < PUBLISH_INTERVAL_MS)
    return;
  lastPublish = now;

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature))
  {
    lcd.clear();
    lcd.print("Sensor Error!");
    return;
  }

  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(20000))) {
    if (subscription == &Light1) {
      int Light1_State = atoi((char *)Light1.lastread);
      digitalWrite(RELAY1, Light1_State);
    }
    if (subscription == &Light2) {
      int Light2_State = atoi((char *)Light2.lastread);
      digitalWrite(RELAY2, Light2_State);
    }
  }

  // LCD Display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp :");
  lcd.print(temperature);
  lcd.print("C");
  lcd.setCursor(0, 1);
  lcd.print("Hum :");
  lcd.print(humidity);
  lcd.print("%");

  // Publish to MQTT
  temperatureFeed.publish(temperature);
  humidityFeed.publish(humidity);

  // ----------- RELAY 1 LOGIC (Temperature) -----------
  if (relay1Mode == "ON")
  {
    digitalWrite(RELAY1, LOW);
    Serial.println("Relay1 MANUAL ON");
  }
  else if (relay1Mode == "OFF")
  {
    digitalWrite(RELAY1, HIGH);
    Serial.println("Relay1 MANUAL OFF");
  }
  else
  {
    // AUTO MODE
    if (temperature >= TEMP_HIGH || temperature <= TEMP_LOW)
    {
      digitalWrite(RELAY1, LOW); // ON
      Serial.println("Relay1 AUTO ON");
    }
    else
    {
      digitalWrite(RELAY1, HIGH); // OFF
      Serial.println("Relay1 AUTO OFF");
    }
  }

  // ----------- RELAY 2 LOGIC (Humidity) -----------
  if (relay2Mode == "ON")
  {
    digitalWrite(RELAY2, LOW);
    Serial.println("Relay2 MANUAL ON");
  }
  else if (relay2Mode == "OFF")
  {
    digitalWrite(RELAY2, HIGH);
    Serial.println("Relay2 MANUAL OFF");
  }
  else
  {
    // AUTO MODE
    if (humidity >= HUM_HIGH || humidity <= HUM_LOW)
    {
      digitalWrite(RELAY2, LOW); // ON
      Serial.println("Relay2 AUTO ON");
    }
    else
    {
      digitalWrite(RELAY2, HIGH); // OFF
      Serial.println("Relay2 AUTO OFF");
    }
  }
}
