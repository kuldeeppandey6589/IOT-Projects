#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_MQTT_Client.h>
#include <lcd.h>

#define AIO_USERNAME "motor251025"
// Motor@123

const String WLAN_SSID = "Robotutor", WLAN_PASS = "Robotutor";
const String AIO_SERVER = "io.adafruit.com", AIO_KEY = "aio_tqEb51rtUjk2URI6QwYBTzUNg1G1";
const int PORT = 1883;

void connect(Adafruit_MQTT_Client &client);
LCD lcd;

void setup()
{
  Serial.begin(9600);
  lcd.println("Welcome!!");
  WiFiClient client;
  Adafruit_MQTT_Client mqtt(&client, AIO_SERVER.c_str(), PORT, AIO_USERNAME, AIO_KEY.c_str());
  Adafruit_MQTT_Subscribe plusFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/plus");
  Adafruit_MQTT_Subscribe minusFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/minus");
  Adafruit_MQTT_Subscribe toggleFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/toggle");
  Adafruit_MQTT_Subscribe playFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/play");

  const uint8_t motor1 = 12, motor2 = 13;
  int step = 0;
  bool direction = true;
  bool isRunning = false;
  pinMode(motor1, OUTPUT);
  pinMode(motor2, OUTPUT);

  mqtt.subscribe(&plusFeed);
  mqtt.subscribe(&minusFeed);
  mqtt.subscribe(&toggleFeed);
  mqtt.subscribe(&playFeed);

  while (true)
  {
    connect(mqtt);
    Adafruit_MQTT_Subscribe *subscription;
    while ((subscription = mqtt.readSubscription(5000)))
    {
      if (subscription == &plusFeed)
      {
        step++;
        step = min(step, 5);
        isRunning = true;
      }
      if (subscription == &minusFeed)
      {
        step--;
        step = max(step, 0);
        isRunning = true;
      }
      if (subscription == &toggleFeed)
      {
        String state = String((char *)toggleFeed.lastread);
        direction = (state == "1");
      }
      if (subscription == &playFeed)
      {
        String state = String((char *)playFeed.lastread);
        isRunning = (state == "1");
      }
    }

    if (!isRunning)
    {
      digitalWrite(motor1, LOW);
      digitalWrite(motor2, LOW);
      lcd.println("Motor Stopped");
    }
    else
    {
      int speed = step * 50;
      if (direction)
      {
        analogWrite(motor1, speed);
        digitalWrite(motor2, LOW);
        lcd.println("Motor Forward\nSpeed: " + String(step));
      }
      else
      {
        digitalWrite(motor1, LOW);
        analogWrite(motor2, speed);
        lcd.println("Motor Backward\nSpeed: " + String(step));
      }
    }
  }
}

void loop()
{
}

void connect(Adafruit_MQTT_Client &mqtt)
{
  bool isConnected = WiFi.isConnected() && mqtt.connected();
  if (isConnected)
  {
    return;
  }
  lcd.println("Connecting...");
  if (!WiFi.isConnected())
  {
    WiFi.begin(WLAN_SSID, WLAN_PASS);
    Serial.println("Connecting with\nWifi");
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    Serial.println("connected");
  }
  if (!mqtt.connected())
  {
    Serial.println("Mqtt disconneted");
    mqtt.disconnect();
    delay(1000);
    mqtt.connect();
    delay(5000);
    if (mqtt.connected())
    {
      Serial.println("Mqtt conneted");
    }
  }
}
