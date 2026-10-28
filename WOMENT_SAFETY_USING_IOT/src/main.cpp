#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Adafruit_MQTT_Client.h>
#include <lcd.h>

#define AIO_USERNAME "woman251030"
// Password: Woman@123

const String WLAN_SSID = "Robotutor", WLAN_PASS = "Robotutor";
const String AIO_SERVER = "io.adafruit.com", AIO_KEY = "aio_HKSF12LptW03JqDLLFfrQU6YZGou";
const int PORT = 1883;

void connect(Adafruit_MQTT_Client &client, LCD &lcd);

void setup()
{
  Serial.begin(9600);
  LCD lcd;
  lcd.println("Welcome");

  const uint8_t alertPin = D0, led = D4;

  pinMode(alertPin, INPUT);
  pinMode(led, OUTPUT);
  WiFiClient client;
  Adafruit_MQTT_Client mqtt(&client, AIO_SERVER.c_str(), PORT, AIO_USERNAME, AIO_KEY.c_str());

  Adafruit_MQTT_Publish alertFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/alert");

  bool lastAlertState = false;

  while (true)
  {
    connect(mqtt, lcd);
    bool alertState = digitalRead(alertPin);
    digitalWrite(led, alertState);
    if (alertState != lastAlertState)
    {
      Serial.println("Alert! state updated.");
      lcd.clearAndPrintln("Alert state:\n" + String(alertState ? "ON" : "OFF"));
      alertFeed.publish(alertState);
      lastAlertState = alertState;
    }
    delay(100);
  }
}

void loop()
{
}

void connect(Adafruit_MQTT_Client &mqtt, LCD &lcd)
{
  bool isConnected = WiFi.isConnected() && mqtt.connected();
  if (isConnected)
  {
    return;
  }

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  Serial.print("Connecting to WiFi");
  lcd.clearAndPrintln("Connecting to\nWiFi...");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  int8_t ret;
  lcd.clearAndPrintln("Connecting to\nServer...");
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0)
  {
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);
    retries--;
  }
}


