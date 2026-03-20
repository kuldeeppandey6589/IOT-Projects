/****************************************************
   AIR & SOUND POLLUTION MONITORING - ADAFRUIT IO

   Board  : ESP8266 (NodeMCU)
   Sensors:
     - DHT11  (Temp & Humidity)
     - MQ135  (Air Quality)
     - Sound Sensor (Digital)

   Cloud  : Adafruit IO
****************************************************/

#include <ESP8266WiFi.h>
#include "AdafruitIO_WiFi.h"
#include "DHT.h"


// ------------ USER CONFIG -----------------
#define WIFI_SSID       "homeiot"
#define WIFI_PASS       "homeiot123"


#define IO_USERNAME     "air081225"
//password : Air@123
#define IO_KEY          "aio_tMFe19OYAHSyxAVpKaOqbUXNr0kV"
// ------------------------------------------


// ----------- Adafruit IO ------------------
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);


// ----------- Feeds ------------------------
AdafruitIO_Feed *temperatureFeed = io.feed("temperature");
AdafruitIO_Feed *humidityFeed    = io.feed("humidity");
AdafruitIO_Feed *airFeed         = io.feed("air-quality");
AdafruitIO_Feed *soundFeed       = io.feed("sound-level");


// ----------- DHT11 CONFIG -----------------
#define DHTPIN  D4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ----------- MQ135 ------------------------
#define MQ135_PIN A0

// ----------- SOUND SENSOR -----------------
#define SOUND_PIN D5

// ----------- TIMING -----------------------
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 15000; // 15s (Adafruit IO limit)

void setup() {
  Serial.begin(115200);
  delay(10);

  pinMode(SOUND_PIN, INPUT);
  dht.begin();

  Serial.println("Connecting to Adafruit IO...");
  io.connect();

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("✅ Adafruit IO Connected!");
}

void loop() {
  io.run();

  if (millis() - lastSend > SEND_INTERVAL) {
    lastSend = millis();
    sendSensorData();
  }
}

// ---------------- MAIN FUNCTION ----------------
void sendSensorData() {

  // Read DHT11
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("❌ DHT Sensor Error");
    return;
  }

  // Read MQ135
  int airQuality = analogRead(MQ135_PIN);

  // Read Sound Sensor (sampling method)
  int soundCount = 0;
  for (int i = 0; i < 200; i++) {
    if (digitalRead(SOUND_PIN) == HIGH) {
      soundCount++;
    }
    delay(1);
  }

  int soundLevel = map(soundCount, 0, 200, 0, 100);
  soundLevel = constrain(soundLevel, 0, 100);

  // ---- Serial Output ----
  Serial.println("------------ SENSOR DATA ------------");
  Serial.print("Temperature : "); Serial.print(temperature); Serial.println(" °C");
  Serial.print("Humidity    : "); Serial.print(humidity);    Serial.println(" %");
  Serial.print("Air Quality : "); Serial.println(airQuality);
  Serial.print("Sound Level : "); Serial.print(soundLevel);  Serial.println(" /100");

  // ---- Send to Adafruit IO ----
  temperatureFeed->save(temperature);
  humidityFeed->save(humidity);
  airFeed->save(airQuality);
  soundFeed->save(soundLevel);

  Serial.println("✅ Data sent to Adafruit IO\n");
}
