#include <WiFi.h>
#include "AdafruitIO_WiFi.h"
#include <DHT.h>
#include <ESP32Servo.h>

// ==========================
// Adafruit IO Credentials
// ==========================
#define IO_USERNAME  "rain030426"
#define IO_KEY       "aio_DhhD61Umyeg12QDQTRJAJxgeffgQ"

// ==========================
// WiFi Credentials
// ==========================
#define WIFI_SSID    "Robotutor"
#define WIFI_PASS    "Robotutor"

// ==========================
// Pin Definitions for ESP32
// ==========================
#define RAIN_SENSOR   34     // ADC pin
#define BUZZER_PIN    18
#define SERVO_PIN     19
#define DHT_PIN       4
#define DHT_TYPE      DHT11

// ==========================
// Objects
// ==========================
DHT dht(DHT_PIN, DHT_TYPE);
Servo servo;

// ==========================
// Adafruit IO Setup
// ==========================
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

AdafruitIO_Feed *temperatureFeed = io.feed("temperature");
AdafruitIO_Feed *humidityFeed    = io.feed("humidity");
AdafruitIO_Feed *rainValueFeed   = io.feed("rain-value");
AdafruitIO_Feed *rainStatusFeed  = io.feed("rain-status");

// ==========================
// Variables
// ==========================
bool lastStateIsRaining = false;
unsigned long lastUpload = 0;
const unsigned long uploadInterval = 10000;

int rainThreshold = 3500;  
// ESP32 ADC range is usually 0 to 4095
// Adjust after testing

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(RAIN_SENSOR, INPUT);

  dht.begin();

  // Servo setup for ESP32
  servo.setPeriodHertz(50);                 // standard 50 Hz servo
  servo.attach(SERVO_PIN, 500, 2400);       // min/max pulse width
  servo.write(0);

  // Better ADC setup
  analogReadResolution(12);                 // 0 to 4095

  Serial.println();
  Serial.println("Connecting to Adafruit IO...");
  io.connect();

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("Adafruit IO Connected!");
}

void loop() {
  io.run();

  // Read rain sensor
  int rainValue = analogRead(RAIN_SENSOR);

  // Adjust logic if needed
  bool isRaining = (rainValue < rainThreshold);

  // Rain state change action
  if (lastStateIsRaining != isRaining) {
    digitalWrite(BUZZER_PIN, HIGH);

    if (isRaining) {
      servo.write(90);
      Serial.println("Rain detected!");
    } else {
      servo.write(0);
      Serial.println("No rain!");
    }

    delay(1000);
    digitalWrite(BUZZER_PIN, LOW);
    lastStateIsRaining = isRaining;
  }

  // Read DHT11
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Serial Monitor output
  Serial.print("Rain Value: ");
  Serial.println(rainValue);

  Serial.print("Rain Status: ");
  Serial.println(isRaining ? "RAINING" : "NO RAIN");

  if (!isnan(temperature) && !isnan(humidity)) {
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
  } else {
    Serial.println("Failed to read from DHT11");
  }

  // Upload to Adafruit IO
  if (millis() - lastUpload > uploadInterval) {
    lastUpload = millis();

    rainValueFeed->save(rainValue);
    rainStatusFeed->save(isRaining ? "Raining" : "No Rain");

    if (!isnan(temperature) && !isnan(humidity)) {
      temperatureFeed->save(temperature);
      humidityFeed->save(humidity);
    }

    Serial.println("Data uploaded to Adafruit IO");
  }

  delay(1000);
}