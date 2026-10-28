/*****************************************************
 SIMPLE POTENTIOMETER + RELAY + ADAFRUIT IO (ESP12E)
 - Relay ON when pot in 0..400
 - Relay OFF when pot > 400
 - Map 0..400 -> 0..250 RPM
 - 16x2 I2C LCD messages (regen ON, motor OFF)
 - ONLY RPM is published to Adafruit IO
 - Publish only on change AND no more than once every 2s
*****************************************************/

#include <ESP8266WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ----- WiFi -----
const char *ssid = "Robotutor";
const char *password = "Robotutor";

// ----- Adafruit IO -----
#define AIO_USERNAME "break261125"
#define AIO_KEY "aio_eeYM92zaLWRGpk8NWHexKKkjoxeg"

// MQTT Server
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client,
                          "io.adafruit.com",
                          1883,
                          AIO_USERNAME,
                          AIO_KEY);

// Feed ONLY for RPM publishing
Adafruit_MQTT_Publish rpmFeed =
  Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/motor_rpm");

// ----- Pins -----
const int POT_PIN = A0;
const int RELAY_PIN = D6;

// ----- LCD -----
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ----- Relay state -----
enum RelayState { RELAY_OFF = 0, RELAY_ON = 1 };
RelayState lastRelayState = RELAY_OFF;

// ----- LCD running-screen state (global so we can reset it) -----
bool showingRunning = false;
unsigned long regenStartMillis = 0;
const unsigned long REGEN_MSG_MS = 2000;  // 2 seconds

// ----- Publishing control -----
int lastPublishedRPM = -1;                 // last RPM value published
unsigned long lastPublishMillis = 0;       // last time we published
const unsigned long PUBLISH_INTERVAL = 2000; // minimum interval between publishes (ms)

void connectMQTT() {
  // Attempt to connect until success. Avoid extremely tight loop by delaying between retries.
  while (!mqtt.connected()) {
    Serial.print("Connecting to Adafruit IO...");
    if (mqtt.connect()) {
      Serial.println("connected!");
      // Reset lastPublishedRPM so first publish after connection can occur immediately if desired
      lastPublishedRPM = -1;
      lastPublishMillis = 0;
    } else {
      Serial.print("Failed, rc=");
      //Serial.print(mqtt.connectErrorString());
      Serial.println(" - retrying in 1s");
      delay(1000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // For ESP8266 NodeMCU: SDA = D2, SCL = D1
  Wire.begin(D2, D1);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Booting...");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWiFi Connected");

  connectMQTT();

  lcd.clear();
}

void loop() {
  // Let the MQTT client do background work
  mqtt.processPackets(10);

  if (!mqtt.connected()) {
    connectMQTT();
  }

  // ----- Read pot -----
  int potValue = analogRead(POT_PIN); // 0..1023 (ESP8266 A0 is 0..1023)

  // ----- Determine relay state -----
  RelayState currentState = (potValue <= 400) ? RELAY_ON : RELAY_OFF;

  // ----- Map pot to RPM -----
  int rpm = 0;
  if (currentState == RELAY_ON) {
    int constrained = constrain(potValue, 0, 400);
    // map so that potValue=400 -> rpm=0 and potValue=0 -> rpm=250
    rpm = map(constrained, 400, 0, 0, 250);
  }

  // ----- Relay control -----
  digitalWrite(RELAY_PIN, (currentState == RELAY_ON) ? HIGH : LOW);

  // ----- LCD Logic -----
  if (currentState == RELAY_ON && lastRelayState == RELAY_OFF) {
    // Transition OFF -> ON
    showingRunning = false;         // ensure running screen re-inits
    regenStartMillis = millis();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Motor Running");
    lcd.setCursor(0, 1);
    lcd.print("RPM: ");
    lcd.print(rpm);
  } else if (currentState == RELAY_ON) {
    // Still ON
    if (millis() - regenStartMillis <= REGEN_MSG_MS) {
      // within regen message window: update RPM on line
      lcd.setCursor(5, 1);  // position after "RPM: "
      lcd.print("    ");    // clear area
      lcd.setCursor(5, 1);
      lcd.print(rpm);
    } else {
      // after 2s, show running screen (initialize once)
      if (!showingRunning) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Motor Running");
        showingRunning = true;
      }
      lcd.setCursor(0, 1);
      // keep the RPM area tidy
      lcd.print("RPM: ");
      if (rpm < 100) lcd.print(" "); // minor alignment (optional)
      lcd.print(rpm);
      lcd.print("   ");
    }
  } else { // currentState == RELAY_OFF
    if (lastRelayState == RELAY_ON) {
      // Transition ON -> OFF: show regen message then OFF screen
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Regeneration ON");
      delay(1000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MOTOR OFF");
      lcd.setCursor(0, 1);
      lcd.print("             ");
    } else {
      // persistently show MOTOR OFF (no constant redraw)
      // we won't change display every loop to reduce flicker
    }
    // Reset running-screen flag to force re-init next time we go ON
    showingRunning = false;
  }

  // Track last state for transition detection
  lastRelayState = currentState;

  // ----- Serial Output -----
  Serial.print("Pot = ");
  Serial.print(potValue);
  Serial.print(" | RPM = ");
  Serial.print(rpm);
  Serial.print(" | Relay = ");
  Serial.println((currentState == RELAY_ON) ? "ON" : "OFF");

  // ----- PUBLISH ONLY RPM (change-based + rate-limited) -----
  unsigned long now = millis();
  bool timeOK = (now - lastPublishMillis) >= PUBLISH_INTERVAL;
  bool changed = (rpm != lastPublishedRPM);

  if (timeOK && changed) {
    if (!rpmFeed.publish(rpm)) {
      Serial.println("Publish failed. Reconnecting and retrying...");
      connectMQTT();
      // Try once more
      if (rpmFeed.publish(rpm)) {
        Serial.println("Publish retry succeeded.");
        lastPublishedRPM = rpm;
        lastPublishMillis = now;
      } else {
        Serial.println("Publish retry failed.");
        // do not update lastPublishedRPM so we'll try again later
      }
    } else {
      Serial.print("Published RPM = ");
      Serial.println(rpm);
      lastPublishedRPM = rpm;
      lastPublishMillis = now;
    }
  }

  // Sleep-ish to avoid very tight-loop; this doesn't affect publish interval enforcement
  delay(250);
}
