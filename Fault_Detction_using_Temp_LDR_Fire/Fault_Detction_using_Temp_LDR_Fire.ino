#include <ESP8266WiFi.h>
#include "AdafruitIO_WiFi.h"
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define LDR_PIN      A0
#define FIRE_PIN     D5
#define DHTPIN       D4
#define BUZZER_PIN   D6
#define RELAY_PIN    D7

#define DHTTYPE      DHT11

#define IO_USERNAME  "fault150326"
#define IO_KEY       "aio_ZuaV18OGUyxbuJhX6tFmZcSsHVCg"

#define WIFI_SSID    "home"
#define WIFI_PASS    "homeiot123"

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27,16,2);

// Adafruit IO feeds
AdafruitIO_Feed *ldrFeed   = io.feed("ldr");
AdafruitIO_Feed *tempFeed  = io.feed("temperature");
AdafruitIO_Feed *humFeed   = io.feed("humidity");
AdafruitIO_Feed *fireFeed  = io.feed("fire");
AdafruitIO_Feed *faultFeed = io.feed("fault-status");

const int LDR_THRESHOLD = 300;
const float TEMP_THRESHOLD = 45.0;

// Upload interval
unsigned long lastUpload = 0;
const unsigned long uploadInterval = 10000;

void setup() {

  pinMode(FIRE_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);

  dht.begin();

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("Industrial");
  lcd.setCursor(0,1);
  lcd.print("Fault System");
  delay(2000);
  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("Connecting...");
  
  io.connect();

  while(io.status() < AIO_CONNECTED){
    io.run();
    delay(500);
  }

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WiFi Connected");
  lcd.setCursor(0,1);
  lcd.print("Adafruit Ready");
  delay(2000);
  lcd.clear();
}

void loop() {

  io.run();

  int ldrValue = analogRead(LDR_PIN);
  int fireValue = digitalRead(FIRE_PIN);

  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();

  bool fireDetected = (fireValue == LOW);
  bool tempFault = (!isnan(temp) && temp > TEMP_THRESHOLD);
  bool ldrFault = (ldrValue < LDR_THRESHOLD);

  bool anyFault = fireDetected || tempFault || ldrFault;

  String faultStatus = "NORMAL";

  if(fireDetected){
    faultStatus = "FIRE ALERT";
  }
  else if(tempFault){
    faultStatus = "OVERHEAT";
  }
  else if(ldrFault){
    faultStatus = "LIGHT FAULT";
  }

  // Buzzer + Relay
  if(anyFault){
    digitalWrite(BUZZER_PIN,HIGH);
    digitalWrite(RELAY_PIN,LOW);
  }
  else{
    digitalWrite(BUZZER_PIN,LOW);
    digitalWrite(RELAY_PIN,HIGH);
  }

  // LCD Display
  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("T:");
  lcd.print(temp,1);
  lcd.print("C");

  lcd.setCursor(9,0);
  lcd.print("L:");
  lcd.print(ldrValue,0);

  lcd.setCursor(0,1);

  if(fireDetected){
    lcd.print("FIRE ALERT");
  }
  else if(tempFault){
    lcd.print("OVERHEAT");
  }
  else if(ldrFault){
    lcd.print("LIGHT FAULT");
  }
  else{
    lcd.print("NORMAL");
  }

  // Upload to Adafruit IO
  if(millis() - lastUpload >= uploadInterval){

    lastUpload = millis();

    ldrFeed->save(ldrValue);

    if(!isnan(temp)){
      tempFeed->save(temp);
    }

    if(!isnan(hum)){
      humFeed->save(hum);
    }

    fireFeed->save(fireDetected ? 1 : 0);
    faultFeed->save(faultStatus);
  }

  delay(1000);
}