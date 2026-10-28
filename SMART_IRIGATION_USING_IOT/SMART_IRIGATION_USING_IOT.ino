/***************************************************
 SMART IRRIGATION SYSTEM USING ESP32 + IoT
 Soil Moisture Sensor + Relay + Adafruit IO
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
#define AIO_KEY "aio_DjUZ56XlhkahKiv4687oX7aBDGWB"

/********* Pin Definitions *********/
#define SOIL_PIN 34   // ESP32 ADC pin
#define RELAY_PIN 27  // Relay control pin
/********* DHT11 Definitions *********/
#define DHTPIN 26  // GPIO pin for DHT11
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);


WiFiClient client;

Adafruit_MQTT_Client mqtt(&client,
                          AIO_SERVER,
                          AIO_SERVERPORT,
                          AIO_USERNAME,
                          AIO_KEY);

/********* Adafruit IO Feeds *********/
Adafruit_MQTT_Publish soilFeed =
  Adafruit_MQTT_Publish(&mqtt,
                        AIO_USERNAME "/feeds/soil");

Adafruit_MQTT_Subscribe motorFeed =
  Adafruit_MQTT_Subscribe(&mqtt,
                          AIO_USERNAME "/feeds/motor");

Adafruit_MQTT_Publish tempFeed =
  Adafruit_MQTT_Publish(&mqtt,
                        AIO_USERNAME "/feeds/temperature");

Adafruit_MQTT_Publish humFeed =
  Adafruit_MQTT_Publish(&mqtt,
                        AIO_USERNAME "/feeds/humidity");

/********* Function Prototype *********/
void MQTT_connect();

void setup() {
  Serial.begin(9600);
  dht.begin();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // Relay OFF initially

  Serial.print("Connecting to WiFi");
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

  // Read soil moisture
  // Read DHT11
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();  // Celsius

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print(" °C  |  Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    tempFeed.publish(temperature);
    humFeed.publish(humidity);
  }

  int soilValue = analogRead(SOIL_PIN);
  Serial.print("Soil Moisture Value: ");
  Serial.println(soilValue);

  // ✅ FIXED LINE (important)
  soilFeed.publish((int32_t)soilValue);

  // Manual control from Adafruit IO
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(100))) {
    if (subscription == &motorFeed) {
      int motorState = atoi((char *)motorFeed.lastread);

      if (motorState == 1) {
        digitalWrite(RELAY_PIN, HIGH);  // Motor ON
        Serial.println("Motor ON (Manual)");
      } else {
        digitalWrite(RELAY_PIN, LOW);  // Motor OFF
        Serial.println("Motor OFF (Manual)");
      }
    }
  }

  // Automatic control logic
  if (soilValue == 4095) {  // Dry soil
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("Motor ON (Auto)");
  } else {  // Wet soil
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("Motor OFF (Auto)");
  }

  delay(3000);  // Delay between readings
}

void MQTT_connect() {
  if (mqtt.connected()) return;

  Serial.print("Connecting to Adafruit IO");
  int8_t ret;

  while ((ret = mqtt.connect()) != 0) {
    Serial.print(".");
    mqtt.disconnect();
    delay(5000);
  }
  Serial.println("\nAdafruit IO Connected");
}

