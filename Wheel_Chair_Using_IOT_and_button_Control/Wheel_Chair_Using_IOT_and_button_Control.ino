
/***************************************************
   IoT CAR WITH ESP32 + ADAFRUIT IO (4 BUTTONS)
   - Control car from Adafruit IO dashboard
   - Buttons: Forward, Backward, Left, Right

   Added:
   - DHT11 reading
   - Publish temperature & humidity to Adafruit IO feeds

   NEW:
   - Voice control via HC-05 Bluetooth module
   - Commands: F,B,L,R,S  (Forward, Backward, Left, Right, Stop)

   UPDATED (to avoid Adafruit IO throttle):
   - Publish DHT data only every 60s
   - Publish only when value changes (change-based publishing)

   BOARD: ESP32 Dev Module
****************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include "AdafruitIO_WiFi.h"
#include "DHT.h"
#include <math.h>

// ------------------ USER CONFIG -------------------
#define IO_USERNAME  "chair051225"
#define IO_KEY       "aio_abDh53qTAUtma0fvrPqEr6nAYrvV"

#define WIFI_SSID    "homeiot"
#define WIFI_PASS    "homeiot123"
// --------------------------------------------------

// Create Adafruit IO instance
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// 4 button feeds on Adafruit IO
AdafruitIO_Feed *forwardFeed  = io.feed("forward");
AdafruitIO_Feed *backwardFeed = io.feed("backward");
AdafruitIO_Feed *leftFeed     = io.feed("left");
AdafruitIO_Feed *rightFeed    = io.feed("right");

// ---------- DHT CONFIG & FEEDS --------------------
#define DHTPIN 4        // GPIO4 for DHT11 data
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// New Adafruit IO feeds for temperature and humidity
AdafruitIO_Feed *tempFeed = io.feed("dht-temperature");
AdafruitIO_Feed *humFeed  = io.feed("dht-humidity");

// Publish interval (ms)  ***SLOWER TO SAVE QUOTA***
const unsigned long DHT_PUBLISH_INTERVAL = 60000UL; // 60 seconds
unsigned long lastDHTPublish = 0;

// Change-based publishing variables
float lastTemp = NAN;
float lastHum  = NAN;
const float TEMP_DELTA = 0.5;   // publish only if temp changed >= 0.5°C
const float HUM_DELTA  = 1.0;   // publish only if hum changed >= 1%
// --------------------------------------------------

// ------------------ PIN DEFINITIONS ---------------
// Motor driver pins for ESP32 (change to your wiring)
#define IN1 33   // Motor 1 control pin 1
#define IN2 27   // Motor 1 control pin 2
#define IN3 26   // Motor 2 control pin 1
#define IN4 25   // Motor 2 control pin 2

#define LED_PIN 2   // On-board LED on most ESP32 dev boards
// --------------------------------------------------

// ----------- BLUETOOTH (HC-05) CONFIG -------------
// Wiring (ESP32):
// HC-05 TX -> GPIO16 (BT_RX)
// HC-05 RX -> GPIO17 (BT_TX)  (use voltage divider on HC-05 RX)
// HC-05 VCC -> 5V (depending on module)
// HC-05 GND -> GND

#define BT_RX 18   // ESP32 receives from HC-05 TX
#define BT_TX 19   // ESP32 transmits to HC-05 RX

// Use UART1 of ESP32
HardwareSerial BTSerial(1);
// --------------------------------------------------

void goForward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  Serial.println("Going Forward");
}

void goBackward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  Serial.println("Going Backward");
}

void turnLeft() {
  // Left: left motor stop, right motor forward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  Serial.println("Turning Left");
}

void turnRight() {
  // Right: right motor stop, left motor forward
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  Serial.println("Turning Right");
}

void stopCar() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  Serial.println("Stop");
}

// ------------- FEED HANDLERS ----------------------
void handleForward(AdafruitIO_Data *data) {
  Serial.print("Forward feed: ");
  Serial.println(data->toString());

  if (data->toString() == "ON") {
    goForward();
  } else {
    stopCar();
  }
}

void handleBackward(AdafruitIO_Data *data) {
  Serial.print("Backward feed: ");
  Serial.println(data->toString());

  if (data->toString() == "ON") {
    goBackward();
  } else {
    stopCar();
  }
}

void handleLeft(AdafruitIO_Data *data) {
  Serial.print("Left feed: ");
  Serial.println(data->toString());

  if (data->toString() == "ON") {
    turnLeft();
  } else {
    stopCar();
  }
}

void handleRight(AdafruitIO_Data *data) {
  Serial.print("Right feed: ");
  Serial.println(data->toString());

  if (data->toString() == "ON") {
    turnRight();
  } else {
    stopCar();
  }
}
// --------------------------------------------------

// ----------- BLUETOOTH COMMAND HANDLER ------------
void handleBTCommand(char cmd) {
  Serial.print("BT Command: ");
  Serial.println(cmd);

  // F = Forward, B = Backward, L = Left, R = Right, S = Stop
  switch (cmd) {
    case 'F':
    case 'f':
      goForward();
      break;

    case 'B':
    case 'b':
      goBackward();
      break;

    case 'L':
    case 'l':
      turnLeft();
      break;

    case 'R':
    case 'r':
      turnRight();
      break;

    case 'S':
    case 's':
      stopCar();
      break;

    default:
      // Unknown command -> stop for safety
      stopCar();
      break;
  }
}
// --------------------------------------------------

void publishDHT() {
  // Only send if actually connected to Adafruit IO
  if (io.status() != AIO_CONNECTED) {
    Serial.println("Not connected to IO, skip DHT publish");
    return;
  }

  // Read temperature (Celsius) and humidity
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // Check if any reads failed
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Check if change is significant enough to publish
  bool sendTemp = isnan(lastTemp) || (fabs(t - lastTemp) >= TEMP_DELTA);
  bool sendHum  = isnan(lastHum)  || (fabs(h - lastHum) >= HUM_DELTA);

  if (!sendTemp && !sendHum) {
    Serial.println("DHT change small, not publishing (saving quota)");
    return;
  }

  // Print locally
  Serial.print("DHT -> Temp: ");
  Serial.print(t);
  Serial.print(" *C, Hum: ");
  Serial.print(h);
  Serial.println(" %");

  // Publish only changed values
  if (sendTemp) {
    tempFeed->save(t);
    lastTemp = t;
  }

  if (sendHum) {
    humFeed->save(h);
    lastHum = h;
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  stopCar();                   // motors off
  digitalWrite(LED_PIN, LOW);  // LED OFF (change to HIGH if inverted on your board)

  // Start Bluetooth serial on UART1
  BTSerial.begin(9600, SERIAL_8N1, BT_RX, BT_TX);
  Serial.println();
  Serial.println("HC-05 Bluetooth started at 9600 baud");
  Serial.println("Connecting to Adafruit IO...");

  // Set feed handlers for motor control
  forwardFeed->onMessage(handleForward);
  backwardFeed->onMessage(handleBackward);
  leftFeed->onMessage(handleLeft);
  rightFeed->onMessage(handleRight);

  // Connect to Adafruit IO
  io.connect();

  // Initialize DHT sensor
  dht.begin();

  // Wait for connection
  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("Adafruit IO Connected!");
  digitalWrite(LED_PIN, HIGH);   // LED ON when connected

  // Subscribe to current values so we get last state
  forwardFeed->get();
  backwardFeed->get();
  leftFeed->get();
  rightFeed->get();

  // Also request the latest values for the new feeds (optional)
  tempFeed->get();
  humFeed->get();

  // Initialize timer
  lastDHTPublish = millis();
}

void loop() {
  // Keep Adafruit IO connection alive and process messages
  io.run();

  // Check Bluetooth for voice commands
  if (BTSerial.available()) {
    char cmd = BTSerial.read();
    handleBTCommand(cmd);
  }

  // Periodically read & publish DHT
  unsigned long now = millis();
  if (now - lastDHTPublish >= DHT_PUBLISH_INTERVAL) {
    lastDHTPublish = now;
    publishDHT();
  }
}
