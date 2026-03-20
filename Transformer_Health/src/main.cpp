#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFi.h>   // <-- ESP32 WiFi library
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

#define AIO_USERNAME "Robotutor250525"
const char* WLAN_SSID = "Robotutor";
const char* WLAN_PASS = "Robotutor";
const char* AIO_SERVER = "io.adafruit.com";
const char* AIO_KEY = "aio_wOez84qGEddjcryDiHh0UjLnIcW7";
const int PORT = 1883;

void longBeep(uint8_t buzzer);
void beep(uint8_t buzzer);
void connect(Adafruit_MQTT_Client &client, LiquidCrystal_I2C &lcd);

void setup() {
  Serial.begin(115200);

  // --- Pin mapping for ESP32 (use GPIO numbers) ---
  const uint8_t voltagePin = 34;   // ADC1 channel (safe for analogRead)
  const uint8_t sw1 = 18;          // GPIO18
  const uint8_t sw2 = 19;          // GPIO19
  const uint8_t buzzer = 23;       // GPIO23
  const uint8_t load1Pin = 25;     // GPIO25
  const uint8_t load2Pin = 26;     // GPIO26
  const uint8_t fan = 27;          // GPIO27
  const uint8_t dhtPin = 2;        // GPIO4 for DHT11

  
  pinMode(voltagePin, INPUT);
  pinMode(sw1, INPUT);
  pinMode(sw2, INPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(load1Pin, OUTPUT);
  pinMode(load2Pin, OUTPUT);
  pinMode(fan, OUTPUT);

  digitalWrite(load1Pin, HIGH);
  digitalWrite(load2Pin, HIGH);
  digitalWrite(fan, HIGH);

  // ---- I2C LCD setup (address 0x27, 16x2) ----
  LiquidCrystal_I2C lcd(0x27, 16, 2);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Welcome!!");

  DHT dht(dhtPin, DHT11);
  bool load1On = false, load2On = false;

  WiFiClient client;
  Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, PORT, AIO_USERNAME, AIO_KEY);

  Adafruit_MQTT_Publish tempFeed(&mqtt, AIO_USERNAME "/feeds/temp");
  Adafruit_MQTT_Publish humidityFeed(&mqtt, AIO_USERNAME "/feeds/humidity");
  Adafruit_MQTT_Publish voltageFeed(&mqtt, AIO_USERNAME "/feeds/voltage");

  uint16_t lastTempFeed = 0, lastHumidityFeed = 0, lastVoltageFeed = 0;

  while (true) {
    connect(mqtt, lcd);

    int temp = dht.readTemperature();
    int humidity = dht.readHumidity();
    int rawADC = analogRead(voltagePin);
    int voltage = map(rawADC, 0, 4095, 140, 300);  // ESP32 ADC is 12-bit

    String text = "T:" + String(temp) + " H:" + String(humidity) + " V:" + String(voltage);

    if (digitalRead(sw1)) {
      load1On = !load1On;
      beep(buzzer);
      while (digitalRead(sw1));
    }
    if (digitalRead(sw2)) {
      load2On = !load2On;
      beep(buzzer);
      while (digitalRead(sw2));
    }

    digitalWrite(load1Pin, !load1On);
    digitalWrite(load2Pin, !load2On);
    digitalWrite(fan, temp <= 50);
    if (temp > 50) {
      longBeep(buzzer);
    }

    // ---- LCD conditions ----
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(text);

    if (load1On && load2On) {
      lcd.setCursor(0, 1);
      lcd.print("Overload      ");
      longBeep(buzzer);
      load1On = false;
      load2On = false;
    } else if (voltage < 190) {
      lcd.setCursor(0, 1);
      lcd.print("Under Voltage ");
      longBeep(buzzer);
    } else if (voltage > 250) {
      lcd.setCursor(0, 1);
      lcd.print("Over Voltage  ");
      longBeep(buzzer);
    } else {
      lcd.setCursor(0, 1);
      lcd.print("F:");
      lcd.print(temp > 50 ? "ON " : "OF ");
      lcd.print("L1:");
      lcd.print(load1On ? "ON " : "OF ");
      lcd.print("L2:");
      lcd.print(load2On ? "ON" : "OF");
    }

    // ---- MQTT publish ----
    if (lastTempFeed != temp) {
      tempFeed.publish(temp);
      lastTempFeed = temp;
    }
    if (lastHumidityFeed != humidity) {
      humidityFeed.publish(humidity);
      lastHumidityFeed = humidity;
    }
    if (abs(lastVoltageFeed - voltage) > 5) {
      voltageFeed.publish(voltage);
      lastVoltageFeed = voltage;
    }

    delay(500);
  }
}

void loop() {}

void longBeep(uint8_t buzzer) {
  digitalWrite(buzzer, HIGH);
  delay(1000);
  digitalWrite(buzzer, LOW);
}

void beep(uint8_t buzzer) {
  digitalWrite(buzzer, HIGH);
  delay(100);
  digitalWrite(buzzer, LOW);
}

void connect(Adafruit_MQTT_Client &mqtt, LiquidCrystal_I2C &lcd) {
  bool isConnected = WiFi.isConnected() && mqtt.connected();
  if (isConnected) return;

  WiFi.begin(WLAN_SSID, WLAN_PASS);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting...");
  lcd.setCursor(0, 1);
  lcd.print("WiFi/MQTT     ");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  int8_t ret;
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT in 5s...");
    mqtt.disconnect();
    delay(5000);
    retries--;
    if (retries == 0) break;
  }
}
