/*******************************
   ESP32 + Adafruit IO + 4 Relay
********************************/

#include <WiFi.h>
#include "AdafruitIO_WiFi.h"
                          
// Adafruit IO Details
#define IO_USERNAME  "home070426"
#define IO_KEY       "aio_Krjv39hXg6ySXp8PnrccXypyNfPj"

// WiFi Details
#define WIFI_SSID    "homeiot"
#define WIFI_PASS    "homeiot123"

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// Relay Pins
#define RELAY1 25
#define RELAY2 26
#define RELAY3 27
#define RELAY4 14

// Active LOW Relay Module
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// Adafruit IO Feeds
AdafruitIO_Feed *relay1 = io.feed("relay1");
AdafruitIO_Feed *relay2 = io.feed("relay2");
AdafruitIO_Feed *relay3 = io.feed("relay3");
AdafruitIO_Feed *relay4 = io.feed("relay4");

// Relay States
bool relay1State = false;
bool relay2State = false;
bool relay3State = false;
bool relay4State = false;

void setRelay(int pin, bool state) {
  digitalWrite(pin, state ? RELAY_ON : RELAY_OFF);
}

bool getStateFromData(AdafruitIO_Data *data) {
  String value = data->toString();
  value.trim();
  value.toUpperCase();

  Serial.print("Received Value: ");
  Serial.println(value);

  if (value == "1" || value == "ON" || value == "TRUE") {
    return true;
  }

  return false;
}

void handleRelay1(AdafruitIO_Data *data) -L'{
  relay1State = getStateFromData(data);
  setRelay(RELAY1, relay1State);
  Serial.println(relay1State ? "Relay 1 ON" : "Relay 1 OFF");
}

void handleRelay2(AdafruitIO_Data *data) {
  relay2State = getStateFromData(data);
  setRelay(RELAY2, relay2State);
  Serial.println(relay2State ? "Relay 2 ON" : "Relay 2 OFF");
}

void handleRelay3(AdafruitIO_Data *data) {
  relay3State = getStateFromData(data);
  setRelay(RELAY3, relay3State);
  Serial.println(relay3State ? "Relay 3 ON" : "Relay 3 OFF");
}

void handleRelay4(AdafruitIO_Data *data) {
  relay4State = getStateFromData(data);
  setRelay(RELAY4, relay4State);
  Serial.println(relay4State ? "Relay 4 ON" : "Relay 4 OFF");
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);

  digitalWrite(RELAY1, RELAY_OFF);
  digitalWrite(RELAY2, RELAY_OFF);
  digitalWrite(RELAY3, RELAY_OFF);
  digitalWrite(RELAY4, RELAY_OFF);

  relay1->onMessage(handleRelay1);
  relay2->onMessage(handleRelay2);
  relay3->onMessage(handleRelay3);
  relay4->onMessage(handleRelay4);

  Serial.println("Connecting to Adafruit IO...");
  io.connect();

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("Adafruit IO Connected");

  relay1->get();
  relay2->get();
  relay3->get();
  relay4->get();
}

void loop() {
  io.run();
}