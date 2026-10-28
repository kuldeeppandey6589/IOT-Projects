/***************************************************
 SMART IRRIGATION SYSTEM USING ESP32 + IoT
 Soil + Water Level + Relay + DHT11 + Adafruit IO
 ***************************************************/

#include <WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "DHT.h"

/********* WiFi Credentials *********/
#define WIFI_SSID "homeiot"
#define WIFI_PASSWORD "homeiot123"

/********* Adafruit IO Credentials *********/
#define AIO_SERVER "io.adafruit.com"
#define AIO_SERVERPORT 1883
#define AIO_USERNAME "soil040126"
//Soil@2026
#define AIO_KEY "aio_DjUZ56XlhkahKiv4687oX7aBDGWB"

/********* Pin Definitions *********/
#define SOIL_PIN 34
#define WATER_LEVEL_PIN 35     // ✅ Water Level Sensor
#define RELAY_PIN 27

/********* DHT11 *********/
#define DHTPIN 26
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

/********* Thresholds *********/
#define SOIL_DRY 3500          // Adjust after testing
#define WATER_FULL 2000        // Adjust after testing

WiFiClient client;

Adafruit_MQTT_Client mqtt(&client,
                          AIO_SERVER,
                          AIO_SERVERPORT,
                          AIO_USERNAME,
                          AIO_KEY);

/********* Feeds *********/
Adafruit_MQTT_Publish soilFeed =
  Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/soil");

Adafruit_MQTT_Subscribe motorFeed =
  Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/motor");

Adafruit_MQTT_Publish tempFeed =
  Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");

Adafruit_MQTT_Publish humFeed =
  Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");

/********* Function *********/
void MQTT_connect();

bool manualMode = false;
int manualMotor = 0;

void setup() {
  Serial.begin(9600);
  dht.begin();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // OFF

  Serial.print("Connecting WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");

  mqtt.subscribe(&motorFeed);
}

void loop() {

  MQTT_connect();

  /******** Read Sensors ********/

  int soilValue = analogRead(SOIL_PIN);
  int waterValue = analogRead(WATER_LEVEL_PIN);

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  Serial.println("---------------------------");
  Serial.print("Soil: ");
  Serial.println(soilValue);

  Serial.print("Water Level: ");
  Serial.println(waterValue);

  /******** Publish ********/

  soilFeed.publish((int32_t)soilValue);

  if (!isnan(humidity) && !isnan(temperature)) {
    tempFeed.publish(temperature);
    humFeed.publish(humidity);
  }

  /******** Manual Control ********/

  Adafruit_MQTT_Subscribe *subscription;

  while ((subscription = mqtt.readSubscription(100))) {

    if (subscription == &motorFeed) {

      manualMotor = atoi((char *)motorFeed.lastread);
      manualMode = true;

      Serial.print("Manual Mode: ");
      Serial.println(manualMotor);
    }
  }

  /******** AUTO CUT-OFF LOGIC ********/

  bool soilDry = soilValue > SOIL_DRY;
  bool waterFull = waterValue > WATER_FULL;

  if (waterFull) {

    // SAFETY CUT OFF
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("Tank FULL → Motor OFF (Safety)");

  }

  else if (manualMode) {

    // Manual control

    if (manualMotor == 1) {
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("Motor ON (Manual)");
    } else {
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("Motor OFF (Manual)");
    }

  }

  else {

    // Auto mode

    if (soilDry) {
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("Motor ON (Auto - Soil Dry)");
    } else {
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("Motor OFF (Auto - Soil Wet)");
    }
  }

  delay(3000);
}

/******** MQTT Connect ********/

void MQTT_connect() {

  if (mqtt.connected()) return;

  Serial.print("Connecting MQTT");

  int8_t ret;

  while ((ret = mqtt.connect()) != 0) {
    Serial.print(".");
    mqtt.disconnect();
    delay(5000);
  }

  Serial.println("\nMQTT Connected");
}
