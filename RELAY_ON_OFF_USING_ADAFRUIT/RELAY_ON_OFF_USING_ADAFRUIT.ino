//realy on off

#include <ESP8266WiFi.h>
#include "AdafruitIO_WiFi.h"

// ========== ADAFRUIT IO DETAILS ==========
#define IO_USERNAME  "relay030526"
//Relay@123
#define IO_KEY       "aio_zblg63TRRlU79LIyfkSxN1sJFMu3"

// ========== WIFI DETAILS ==========
#define WIFI_SSID    "homeiot"
#define WIFI_PASS    "homeiot123"

// ========== ADAFRUIT IO OBJECT ==========
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// ========== ADAFRUIT IO FEED ==========
AdafruitIO_Feed *relayFeed = io.feed("relay");

// ========== RELAY PIN ==========
#define RELAY_PIN D1   // GPIO5

// Most relay modules are Active LOW
#define RELAY_ON  HIGH
#define RELAY_OFF LOW

// ========== AUTO OFF TIMER ==========
bool relayState = true;
unsigned long relayOnTime = 0;
const unsigned long AUTO_OFF_TIME = 10000; // 10 seconds

// ========== RELAY FUNCTION ==========
void setRelay(bool state) {
  relayState = state;

  if (state == true) {
    digitalWrite(RELAY_PIN, RELAY_ON);
    relayOnTime = millis();
    Serial.println("Relay ON");
  } 
  else {
    digitalWrite(RELAY_PIN, RELAY_OFF);
    Serial.println("Relay OFF");
  }
}

// ========== ADAFRUIT IO MESSAGE ==========
void handleRelayMessage(AdafruitIO_Data *data) {
  String command = data->toString();
  command.toUpperCase();

  Serial.print("Received: ");
  Serial.println(command);

  if (command == "ON" || command == "1") {
    setRelay(true);
  } 
  else if (command == "OFF" || command == "0") {
    setRelay(false);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  Serial.print("Connecting to Adafruit IO");

  relayFeed->onMessage(handleRelayMessage);
  io.connect();

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("Adafruit IO Connected");

  relayFeed->get();
}

void loop() {
  io.run();

  if (relayState == true && millis() - relayOnTime >= AUTO_OFF_TIME) {
    setRelay(false);

    // Update Adafruit IO dashboard button to OFF
    relayFeed->save("OFF");

    Serial.println("Relay Auto OFF after 10 seconds");
  }
}