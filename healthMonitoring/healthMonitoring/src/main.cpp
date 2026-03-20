#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Adafruit_MQTT_Client.h>
#include <ECG.hpp>
#include <MAX30100.hpp>
#include <lcd.h>
#include <DHT.h>

#define AIO_USERNAME "health301125"
#define AIO_KEY "aio_vcLr38cbxwIMdj52AuVocOzffJKr"

const String WLAN_SSID = "homeiot";
const String WLAN_PASS = "homeiot123";
const String AIO_SERVER = "io.adafruit.com";
const int PORT = 1883;

bool connect(Adafruit_MQTT_Client &client, LCD &lcd);

void setup()
{
  Serial.begin(115200);
  LCD lcd;

  // default pins (change to match your hardware)
  const uint8_t waterPin = D6;  // Digital
  const uint8_t foodPin  = D7;  // Digital
  const uint8_t helpPin  = D5;  // Digital
  const uint8_t ecgAnalog = A0; // ADC
  const uint8_t dhtPin    = D4; // Digital

  ECG ecg(ecgAnalog);
  DHT dht;
  dht.setup(dhtPin, dht.DHT11);
  MAX30100Wrapper max3000;

  pinMode(waterPin, INPUT);
  pinMode(foodPin, INPUT);
  pinMode(helpPin, INPUT);
  ecg.begin();
  max3000.begin();

  lcd.println("Health monitor");
  delay(1000);

  WiFiClient client;
  Adafruit_MQTT_Client mqtt(&client, AIO_SERVER.c_str(), PORT, AIO_USERNAME, AIO_KEY);

  Adafruit_MQTT_Publish waterFeed = Adafruit_MQTT_Publish(&mqtt, "health301125/feeds/water");
  Adafruit_MQTT_Publish foodFeed  = Adafruit_MQTT_Publish(&mqtt, "health301125/feeds/food");
  Adafruit_MQTT_Publish helpFeed  = Adafruit_MQTT_Publish(&mqtt, "health301125/feeds/help");
  Adafruit_MQTT_Publish ecgBPMFeed    = Adafruit_MQTT_Publish(&mqtt, "health301125/feeds/bpm");
  Adafruit_MQTT_Publish heartRateFeed = Adafruit_MQTT_Publish(&mqtt, "health301125/feeds/pulse");
  Adafruit_MQTT_Publish spO2Feed      = Adafruit_MQTT_Publish(&mqtt, "health301125/feeds/spo2");
  Adafruit_MQTT_Publish tempFeed      = Adafruit_MQTT_Publish(&mqtt, "health301125/feeds/temperature");
  Adafruit_MQTT_Publish humFeed       = Adafruit_MQTT_Publish(&mqtt, "health301125/feeds/humidity");

  bool    lastWater = false, lastFood = false, lastHelp = false;
  uint8_t lastEcgBPM = 0, lastHeartRate = 0, lastSpO2 = 0, lastTemp = 0, lastHum = 0;

  // === NEW: rate-limit analog sensor publishing ===
  unsigned long lastAnalogPublish = 0;
  const unsigned long ANALOG_PUBLISH_INTERVAL = 15000; // 15 seconds

  while (true)
  {
    connect(mqtt, lcd);

    bool waterActive = digitalRead(waterPin);
    bool foodActive  = digitalRead(foodPin);
    bool helpActive  = digitalRead(helpPin);

    if (waterActive != lastWater)
    {
      lastWater = waterActive;
      waterFeed.publish(waterActive);
    }
    if (waterActive)
    {
      lcd.println("Water needed");
      delay(2000);
    }

    if (foodActive != lastFood)
    {
      lastFood = foodActive;
      foodFeed.publish(foodActive);
    }
    if (foodActive)
    {
      lcd.println("Food needed");
      delay(2000);
    }

    if (helpActive != lastHelp)
    {
      lastHelp = helpActive;
      helpFeed.publish(helpActive);
    }
    if (helpActive)
    {
      lcd.println("Help needed");
      delay(2000);
    }

    float ecgBPM = ecg.readBPM();
    float hr     = max3000.getHeartRate();
    float spo2   = max3000.getSpO2();
    float temp   = dht.getTemperature();
    float hum    = dht.getHumidity();

    // === NEW: only publish analog values at most once every 15s ===
    unsigned long now = millis();
    if (now - lastAnalogPublish >= ANALOG_PUBLISH_INTERVAL)
    {
      bool anyPublished = false;

      if (lastEcgBPM != (uint8_t)ecgBPM)
      {
        lastEcgBPM = (uint8_t)ecgBPM;
        ecgBPMFeed.publish((int)ecgBPM);
        anyPublished = true;
      }
      if (lastHeartRate != (uint8_t)hr)
      {
        lastHeartRate = (uint8_t)hr;
        heartRateFeed.publish((int)hr);
        anyPublished = true;
      }
      if (lastSpO2 != (uint8_t)spo2)
      {
        lastSpO2 = (uint8_t)spo2;
        spO2Feed.publish((int)spo2);
        anyPublished = true;
      }
      if (lastTemp != (uint8_t)temp)
      {
        lastTemp = (uint8_t)temp;
        tempFeed.publish(temp);
        anyPublished = true;
      }
      if (lastHum != (uint8_t)hum)
      {
        lastHum = (uint8_t)hum;
        humFeed.publish(hum);
        anyPublished = true;
      }

      if (anyPublished)
      {
        lastAnalogPublish = now; // reset timer only if something was sent
      }
    }

    lcd.println("Temp: " + String((int)temp) + "C Hum: " + String((int)hum) + "%");
    delay(2000);
    lcd.println("ECG BPM: " + String((int)ecgBPM) + "\nHR: " + String((int)hr) + " SpO2: " + String((int)spo2));
    delay(2000);
  }
}

void loop()
{
}

bool connect(Adafruit_MQTT_Client &mqtt, LCD &lcd)
{
  bool isConnected = (WiFi.status() == WL_CONNECTED) && mqtt.connected();
  if (isConnected)
  {
    return isConnected;
  }

  WiFi.begin(WLAN_SSID.c_str(), WLAN_PASS.c_str());
  Serial.println("Connecting with WiFi or Server");
  lcd.println("Connecting...");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("connected");
  lcd.println("Connecting with\nServer");
  mqtt.connect();
  return (WiFi.status() == WL_CONNECTED) && mqtt.connected();
}
