#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "DHT.h"

/************************* Pin Definitions *********************************/
// Define the pin the DHT sensor is connected to
#define DHTPIN 4   // Example: connected to D2 (GPIO 4) on NodeMCU
// Define the type of DHT sensor
#define DHTTYPE DHT11  // Or DHT22, etc.

// Define the LED pin.
// On most ESP8266 boards (NodeMCU), the built-in LED is on D4 (GPIO 2).
#define ledPin 2

/************************* WiFi Access Point *********************************/
// --- FILL IN YOUR WIFI CREDENTIALS ---
#define WLAN_SSID "Robotutor"
#define WLAN_PASS "Robotutor"

/************************* Adafruit.io Setup *********************************/
// --- FILL IN YOUR ADAFRUIT IO CREDENTIALS ---
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "fire011125"
#define AIO_KEY         "aio_CRsT954GhsX4ooMsdAQ0bwqigY3f"

/************ Global Object Definitions (This is the part you were missing) ************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;

// Setup the MQTT client class
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/
// Setup a feed called 'temperature' for publishing.
Adafruit_MQTT_Publish temp = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");

// Setup a feed called 'humidity' for publishing.
Adafruit_MQTT_Publish hum = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");

// Setup a feed called 'onoff' for subscribing to changes.
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/onoff");

/*************************** DHT Sensor Setup ************************************/
// Initialize the DHT sensor object
DHT dht(DHTPIN, DHTTYPE);

/*************************** Sketch Code ************************************/

void setup() {
  Serial.begin(115200);
  Serial.println(F("Adafruit IO DHT Example"));

  // Set LED pin as an output
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // Start with LED off

  // Start the DHT sensor
  dht.begin();

  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());

  // Subscribe to the 'onoffbutton' feed
  mqtt.subscribe(&onoffbutton);
}


void loop() {
  // Ensure the connection to the MQTT server is alive.
  void MQTT_connect();

  // Wait for subscription messages
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(2000))) {
    if (subscription == &onoffbutton) {
      Serial.print(F("Got: "));
      Serial.println((char *)onoffbutton.lastread);

      // Check the value of the button feed
      if (strcmp((char *)onoffbutton.lastread, "ON") == 0) {
        digitalWrite(ledPin, HIGH);
      }
      if (strcmp((char *)onoffbutton.lastread, "OFF") == 0) {
        digitalWrite(ledPin, LOW);
      }
    }
  }

  // Read from DHT sensor
  float h = dht.readHumidity();
  float t = dht.readTemperature(); // Read temperature as Celsius

  // Check if reads failed
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return; // Exit loop early if read failed
  }

  // Publish humidity
  Serial.print(F("\nSending Hum: "));
  Serial.print(h);
  Serial.print("...");
  if (! hum.publish(h)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }

  // Publish temperature
  Serial.print(F("Sending Temp: "));
  Serial.print(t);
  Serial.print("...");
  if (! temp.publish(t)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }

  // Wait a few seconds before publishing again
  delay(10000); // Wait 10 seconds
}


// Function to connect and reconnect as necessary to the MQTT server.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically deep sleep on ESP8266
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}
