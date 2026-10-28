#include <WiFi.h>
#include "AdafruitIO_WiFi.h"
#include "DHT.h"

// =====================================================
// WIFI
// =====================================================

#define WIFI_SSID     "homeiot"
#define WIFI_PASSWORD "homeiot123"

// =====================================================
// ADAFRUIT IO
// =====================================================

#define IO_USERNAME  "Soil041026"
//Soil@123
#define IO_KEY       "aio_Ubri527Lsp4UqdRpzNSucU7pRyz4"

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASSWORD);

// =====================================================
// SOIL SENSORS
// =====================================================

#define SOIL1_PIN 34
#define SOIL2_PIN 35

// Adjust this value according to your soil sensor
#define SOIL_DRY_THRESHOLD 2500

// =====================================================
// RELAY MODULE
// =====================================================

#define RELAY1 25
#define RELAY2 26
#define RELAY3 27
#define RELAY4 14

// Most 4-channel relay modules are ACTIVE LOW
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// =====================================================
// DHT11
// =====================================================

#define DHT_PIN 4
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);

// =====================================================
// ADAFRUIT FEEDS
// =====================================================

// AdafruitIO_Feed *soil1Feed = io.feed("soil1");
// AdafruitIO_Feed *soil2Feed = io.feed("soil2");

AdafruitIO_Feed *temperatureFeed = io.feed("temperature");
AdafruitIO_Feed *humidityFeed = io.feed("humidity");

AdafruitIO_Feed *relay1Feed = io.feed("relay1");
AdafruitIO_Feed *relay2Feed = io.feed("relay2");
AdafruitIO_Feed *relay3Feed = io.feed("relay3");
AdafruitIO_Feed *relay4Feed = io.feed("relay4");

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  // Relay pins
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);

  // Initially OFF
  digitalWrite(RELAY1, RELAY_OFF);
  digitalWrite(RELAY2, RELAY_OFF);
  digitalWrite(RELAY3, RELAY_OFF);
  digitalWrite(RELAY4, RELAY_OFF);

  // DHT
  dht.begin();

  // Connect to Adafruit IO
  Serial.println("Connecting to Adafruit IO...");

  io.connect();

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("Connected to Adafruit IO!");
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  // Keep Adafruit IO connection alive
  io.run();

  // ===================================================
  // READ SOIL SENSORS
  // ===================================================

  int soil1Value = analogRead(SOIL1_PIN);
  int soil2Value = analogRead(SOIL2_PIN);

  Serial.println("--------------------------------");

  Serial.print("Soil 1: ");
  Serial.println(soil1Value);

  Serial.print("Soil 2: ");
  Serial.println(soil2Value);

  // ===================================================
  // SOIL 1 CONTROL
  // ===================================================

  if (soil1Value > SOIL_DRY_THRESHOLD) {

    // Soil 1 is DRY
    digitalWrite(RELAY1, RELAY_ON);
    digitalWrite(RELAY2, RELAY_ON);

    Serial.println("Soil 1 DRY -> Relay 1 & 2 ON");

    relay1Feed->save(1);
    relay2Feed->save(1);

  } else {

    // Soil 1 is WET
    digitalWrite(RELAY1, RELAY_OFF);
    digitalWrite(RELAY2, RELAY_OFF);

    Serial.println("Soil 1 WET -> Relay 1 & 2 OFF");

    relay1Feed->save(0);
    relay2Feed->save(0);
  }

  // ===================================================
  // SOIL 2 CONTROL
  // ===================================================

  if (soil2Value > SOIL_DRY_THRESHOLD) {

    // Soil 2 is DRY
    digitalWrite(RELAY3, RELAY_ON);
    digitalWrite(RELAY4, RELAY_ON);

    Serial.println("Soil 2 DRY -> Relay 3 & 4 ON");

    relay3Feed->save(1);
    relay4Feed->save(1);

  } else {

    // Soil 2 is WET
    digitalWrite(RELAY3, RELAY_OFF);
    digitalWrite(RELAY4, RELAY_OFF);

    Serial.println("Soil 2 WET -> Relay 3 & 4 OFF");

    relay3Feed->save(0);
    relay4Feed->save(0);
  }

  // ===================================================
  // SEND SOIL DATA
  // ===================================================

  // soil1Feed->save(soil1Value);
  // soil2Feed->save(soil2Value);

  // ===================================================
  // READ DHT11
  // ===================================================

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (!isnan(temperature) && !isnan(humidity)) {

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    // Send DHT data to Adafruit IO
    temperatureFeed->save(temperature);
    humidityFeed->save(humidity);

  } else {

    Serial.println("DHT11 reading failed!");
  }

  Serial.println("--------------------------------");

  // Wait before next reading
  delay(20000);
}