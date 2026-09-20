#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// ====== WIFI DETAILS ======
#define WIFI_SSID "home"
#define WIFI_PASS "homeiot123"

// ====== ADAFRUIT IO DETAILS ======
#define AIO_SERVER "io.adafruit.com"
#define AIO_SERVERPORT 1883
#define AIO_USERNAME "spreay250226"
//Spreay@123
// #define AIO_KEY "aio_SWKd02nU1x96hHQx9nGvnVUeuJqh"

// ====== RELAY PIN ======
// D5 = GPIO14
#define RELAY_PIN D5

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Feed: relay
Adafruit_MQTT_Subscribe relayFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/relay");

void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  Serial.print("Connecting to Adafruit IO MQTT... ");

  int8_t ret;
  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying in 5 seconds...");
    mqtt.disconnect();
    delay(5000);
  }

  Serial.println("Connected!");
  mqtt.subscribe(&relayFeed);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(RELAY_PIN, OUTPUT);

  // Active LOW relay:
  // HIGH = OFF
  // LOW  = ON
  digitalWrite(RELAY_PIN, HIGH);  

  connectWiFi();
  connectMQTT();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!mqtt.connected()) {
    connectMQTT();
  }

  mqtt.ping();

  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(200))) {
    if (subscription == &relayFeed) {
      String value = String((char *)relayFeed.lastread);
      value.trim();
      value.toUpperCase();

      Serial.print("Received: ");
      Serial.println(value);

      if (value == "ON" || value == "1") {
        digitalWrite(RELAY_PIN, LOW);   // Relay ON
        Serial.println("Relay ON");
      } 
      else if (value == "OFF" || value == "0") {
        digitalWrite(RELAY_PIN, HIGH);  // Relay OFF
        Serial.println("Relay OFF");
      } 
      else {
        Serial.println("Invalid command");
      }
    }
  }
}