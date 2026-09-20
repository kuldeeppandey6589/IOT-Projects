#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SinricPro.h>
#include <SinricProSwitch.h>

#define WIFI_SSID     "home"
#define WIFI_PASS     "homeiot12"

// ==========================
// SinricPro Credentials
// ==========================
#define APP_KEY       "8a802c62-05fd-4d26-b583-a6260a01fd6f"
#define APP_SECRET    "2dc13dda-950f-4375-9c83-1b02eceb93eb-510c3a17-27f3-4c8f-b991-214301cb31f1"

// ==========================
// Device IDs
// ==========================
#define DEVICE_ID_1   "69dc04bb52800e7ce36580c7"
#define DEVICE_ID_2   "69bffbfbc2dbd7108b43de4d"
#define DEVICE_ID_3   "69bffc16dafb005af4dacb2b"

// ==========================
// Relay Pins
// ==========================
#define RELAY_PIN_1   D2
#define RELAY_PIN_2   D3
#define RELAY_PIN_3   D4

// Active LOW relay module
void setRelay(int relayPin, bool state) {
  digitalWrite(relayPin, state ? LOW : HIGH);   // ON = LOW, OFF = HIGH
}

bool onPowerState(const String &deviceId, bool &state) {
  Serial.printf("Device %s turned %s\r\n", deviceId.c_str(), state ? "ON" : "OFF");

  if (deviceId == DEVICE_ID_1) {
    setRelay(RELAY_PIN_1, state);
  } 
  else if (deviceId == DEVICE_ID_2) {
    setRelay(RELAY_PIN_2, state);
  } 
  else if (deviceId == DEVICE_ID_3) {
    setRelay(RELAY_PIN_3, state);
  }

  return true;
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN_1, OUTPUT);
  pinMode(RELAY_PIN_2, OUTPUT);
  pinMode(RELAY_PIN_3, OUTPUT);

  // All relays OFF at start
  digitalWrite(RELAY_PIN_1, HIGH);
  digitalWrite(RELAY_PIN_2, HIGH);
  digitalWrite(RELAY_PIN_3, HIGH);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());

  SinricProSwitch &mySwitch1 = SinricPro[DEVICE_ID_1];
  SinricProSwitch &mySwitch2 = SinricPro[DEVICE_ID_2];
  SinricProSwitch &mySwitch3 = SinricPro[DEVICE_ID_3];

  mySwitch1.onPowerState(onPowerState);
  mySwitch2.onPowerState(onPowerState);
  mySwitch3.onPowerState(onPowerState);

  SinricPro.begin(APP_KEY, APP_SECRET);
  Serial.println("SinricPro started");
}

void loop() {
  SinricPro.handle();
}