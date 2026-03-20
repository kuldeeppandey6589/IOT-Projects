#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <DHT.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"

// -------------------- WiFi & Adafruit IO --------------------
#define WIFI_SSID     "Robotutor"
#define WIFI_PASS     "Robotutor"

#define IO_SERVER     "io.adafruit.com"
#define IO_SERVERPORT 1883
#define IO_USERNAME   "health221125"
//Password : Health@123

#define IO_KEY        "aio_IohU02xdzHXEegbcmiYc1Pa5EaJd"

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, IO_SERVER, IO_SERVERPORT, IO_USERNAME, IO_KEY);

// Feeds
Adafruit_MQTT_Publish feed_bpm      = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/bpm");
Adafruit_MQTT_Publish feed_spo2     = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/spo2");
Adafruit_MQTT_Publish feed_temp     = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish feed_humidity = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/humidity");

// -------------------- Sensors --------------------
#define DHTPIN D5
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

PulseOximeter pox;
uint32_t lastReport = 0;

void onBeatDetected() {
  Serial.println("Beat detected!");
}

// -------------------- Setup --------------------
void setup() {
  Serial.begin(115200);
  delay(10);

  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // Start DHT
  dht.begin();

  // Start MAX30100
  if (!pox.begin()) {
    Serial.println("MAX30100 FAILED");
    while (1);
  }
  pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
  pox.setOnBeatDetectedCallback(onBeatDetected);
}

// -------------------- MQTT Connection --------------------
void connectToMQTT() {
  while (mqtt.connected() == false) {
    Serial.print("Connecting to Adafruit IO...");
    int8_t ret = mqtt.connect();

    if (ret == 0) { Serial.println("Connected!"); break; }

    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying in 5 seconds...");
    delay(5000);
  }
}

// -------------------- Main Loop --------------------
void loop() {
  // Update pulse sensor
  pox.update();

  // Maintain MQTT connection
  if (!mqtt.connected()) {
    connectToMQTT();
  }
  mqtt.processPackets(10);

  // Send data every 2 seconds
  if (millis() - lastReport > 2000) {

    float bpm = pox.getHeartRate();
    float spo2 = pox.getSpO2();

    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();

    Serial.println("\n--- Sending to Adafruit IO ---");
    Serial.println("BPM : " + String(bpm));
    Serial.println("SpO2: " + String(spo2));
    Serial.println("Temp: " + String(temp));
    Serial.println("Hum : " + String(hum));

    feed_bpm.publish(bpm);
    feed_spo2.publish(spo2);
    feed_temp.publish(temp);
    feed_humidity.publish(hum);

    lastReport = millis();
  }
}
