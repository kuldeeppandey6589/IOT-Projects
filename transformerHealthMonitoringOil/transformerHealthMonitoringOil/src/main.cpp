#include <Arduino.h>
#include <lcd.h>
#include <WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <NewPing.h>
#include <limits.h>
#include <DHT.h>

#define AIO_USERNAME "Transformer251125"
// Transformer@123

const uint8_t POT1 = 34, POT2 = 35; // ADC1_CH6 (input only)
const uint8_t LOAD1 = 4, LOAD2 = 5, SW1 = 14, SW2 = 15;

const uint8_t TRIG_PIN = 26;
const uint8_t ECHO_PIN = 27;
NewPing sonar(TRIG_PIN, ECHO_PIN, 200); // max distance 200cm
 
const uint8_t BUZZER_PIN = 25; // PWM-capable GPIO

const String WLAN_SSID = "Robotutor", WLAN_PASS = "Robotutor";
const String AIO_SERVER = "io.adafruit.com", AIO_KEY = "aio_KlQx80c3B2DXlvfsHnwyKXBgUj3k"; // placeholder
const int PORT = 1883;

const float OIL_TANK_HEIGHT_CM = 6.0; // editable

void connect(Adafruit_MQTT_Client &mqtt, LCD &lcd);
void longBeep();
void shortBeep();
float readOilLevelPercent();

void setup()
{
  Serial.begin(115200);
  DHT dht;
  dht.setup(18, dht.DHT11);

  pinMode(LOAD1, OUTPUT);
  pinMode(LOAD2, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  // NewPing handles TRIG/ECHO

  LCD lcd(20, 4);
  lcd.println("Welcome!!");

  WiFiClient client;
  Adafruit_MQTT_Client mqtt(&client, AIO_SERVER.c_str(), PORT, AIO_USERNAME, AIO_KEY.c_str());

  // Feeds
  Adafruit_MQTT_Publish tempFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/oil-temp");
  Adafruit_MQTT_Publish levelFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/oil-level");
  Adafruit_MQTT_Publish alarmFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/alarm");
  Adafruit_MQTT_Publish currentFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/current");
  Adafruit_MQTT_Publish voltageFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/voltage");
  Adafruit_MQTT_Publish overloadFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/overload");

  // last sent cache
  int lastTemp = INT_MIN;
  int lastLevel = INT_MIN;
  int lastAlarm = -1;
  int lastVoltage = INT_MIN;
  int lastCurrent = INT_MIN;
  bool lastOverload = false;
  bool load1On = false, load2On = false;

  delay(2000);

  while (true)
  {
    connect(mqtt, lcd);

    float tempC = dht.getTemperature();
    float levelPercentage = readOilLevelPercent();
    int voltage = map(analogRead(POT1), 0, 4095, 180, 260);
    int current = map(analogRead(POT2), 0, 4095, 1, 30);

    if (digitalRead(SW1))
    {
      load1On = !load1On;
      while (digitalRead(SW1));
    }

    if (digitalRead(SW2))
    {
      load2On = !load2On;
      while (digitalRead(SW2))
        ;
    }

    bool overload = load1On + load2On > 1;

    // Basic rules
    bool overheating = tempC > 40.0;        // threshold for oil overheat
    bool lowLevel = levelPercentage < 25.0; // low oil warning
    bool critical = overheating || (levelPercentage < 10.0);

    String line1 = "Temp:" + String((int)round(tempC)) + "C Level:" + String((int)round(levelPercentage)) + "%\n";
    String line2 = "Volt:" + String(voltage) + "V, Current:" + String(current) + "A\n";
    String line3 = "LOAD1:" + String(load1On ? "ON " : "OFF  ") + " LOAD2:" + String(load2On ? "ON " : "OFF ") + "\n";

    lcd.println(line1 + line2 + line3);

    digitalWrite(LOAD1, load1On);
    digitalWrite(LOAD2, load2On);

    if (critical)
    {
      longBeep();
      lcd.println(line1 + line2 + line3 + String(overheating ? "Overheat" : "Oil level very low"));
      load1On = false;
      load2On = false;
    }
    else if (lowLevel || overload)
    {
      shortBeep();
      lcd.println(line1 + line2 + line3 + String( lowLevel ? "Oil level low" : "Overload"));
      load1On = false;
      load2On = false;
    }

    if(overload){
      shortBeep();
      lcd.println(line1 + line2 + line3 +  "Overload");
      delay(3000);
    }

    // MQTT publish on change (int granularity to reduce noise)
    int tInt = (int)round(tempC);
    int lInt = (int)round(levelPercentage);
    int alarmCode = critical ? 2 : (overheating || lowLevel ? 1 : 0);

    if (lastTemp != tInt)
    {
      tempFeed.publish(tInt);
      lastTemp = tInt;
    }
    if (lastLevel != lInt)
    {
      levelFeed.publish(lInt);
      lastLevel = lInt;
    }
    if (lastAlarm != alarmCode)
    {
      alarmFeed.publish(alarmCode);
      lastAlarm = alarmCode;
    }
    if (lastVoltage != voltage)
    {
      voltageFeed.publish(voltage);
      lastVoltage = voltage;
    }
    if (lastCurrent != current)
    {
      currentFeed.publish(current);
      lastCurrent = current;
    }
    if (lastOverload != overload)
    {
      overloadFeed.publish(overload);
      lastOverload = overload;
    }

    delay(2000);
  }
}

void loop() {}

float readOilLevelPercent()
{
  // Measure distance using NewPing helper
  unsigned int distCm = sonar.ping_cm();
  if (distCm == 0)
  {
    // no echo; return mid value to avoid spikes
    return 50.0f;
  }

  // Simple calculation: level% = 100 * (1 - distance/height)
  float pct = 100.0f * (1.0f - ((float)distCm / OIL_TANK_HEIGHT_CM));
  if (pct < 0)
    pct = 0;
  if (pct > 100)
    pct = 100;
  return pct;
}

void longBeep()
{
  digitalWrite(BUZZER_PIN, HIGH);
  delay(600);
  digitalWrite(BUZZER_PIN, LOW);
}

void shortBeep()
{
  digitalWrite(BUZZER_PIN, HIGH);
  delay(120);
  digitalWrite(BUZZER_PIN, LOW);
}

void connect(Adafruit_MQTT_Client &mqtt, LCD &lcd)
{
  bool isConnected = WiFi.isConnected() && mqtt.connected();
  if (isConnected)
  {
    return;
  }

  WiFi.begin(WLAN_SSID.c_str(), WLAN_PASS.c_str());
  lcd.println("Connecting with\nWifi or Server");

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000)
  {
    delay(2000);
    Serial.print(".");
  }
  Serial.println();

  int8_t ret;

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0 && retries > 0)
  {
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);
    retries--;
  }
}






