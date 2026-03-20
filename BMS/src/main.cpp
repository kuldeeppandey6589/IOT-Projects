#include <Arduino.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <Buzzer.hpp>

enum State
{
  OFF,
  CHARGE,
  FAST_CHARGE,
  DISCHARGE
};

float readBatteryVoltage();
float readCurrent();
float readInputVoltage(uint8_t pin);

void setup()
{
  const uint8_t button1 = 2, button2 = 3, button3 = 4;
  const uint8_t inputVoltagePin = A2, shortCircuitPin = A3;
  const uint8_t chargeRelay = 6, dischargeRelay = 5, chargeIndicator = 9, fullIndicator = 10, fan = 13;

  pinMode(chargeRelay, OUTPUT);
  pinMode(dischargeRelay, OUTPUT);
  pinMode(chargeIndicator, OUTPUT);
  pinMode(fullIndicator, OUTPUT);
  pinMode(fan, OUTPUT);
  digitalWrite(chargeRelay, HIGH);
  digitalWrite(dischargeRelay, HIGH);

  pinMode(button1, INPUT);
  pinMode(button2, INPUT);
  pinMode(button3, INPUT);

  pinMode(inputVoltagePin, INPUT);
  pinMode(shortCircuitPin, INPUT);

  DHT dht;
  dht.setup(8, dht.DHT11);
  Buzzer buzzer(7, 100);
  buzzer.beep();
  LCD lcd(20, 4);
  lcd.println("Welcome to the BMS.");

  State state = OFF;

  while (true)
  {

    float batteryVoltage = readBatteryVoltage();
    int temp = dht.getTemperature();
    int humidity = dht.getHumidity();
    float inputVoltage = readInputVoltage(inputVoltagePin);
    bool isShortCircuit = digitalRead(shortCircuitPin);
    String lcdText = "B.V.:" + String(batteryVoltage) + " I.V:" + String(inputVoltage) + "\nTemp: " + String(temp) + "C, Humi: " + String(humidity);
    if (digitalRead(button1))
    {
      while (digitalRead(button1))
        ;
      buzzer.beep();
      if (state == CHARGE)
        state = OFF;
      else
        state = CHARGE;
    }
    if (digitalRead(button2))
    {
      while (digitalRead(button2))
        ;
      buzzer.beep();
      if (state == FAST_CHARGE)
        state = OFF;
      else
        state = FAST_CHARGE;
    }
    if (digitalRead(button3))
    {
      while (digitalRead(button3))
        ;
      buzzer.beep();
      if (state == DISCHARGE)
        state = OFF;
      else
        state = DISCHARGE;
    }

    switch (state)
    {
    case OFF:
    {
      digitalWrite(chargeRelay, HIGH);
      digitalWrite(dischargeRelay, HIGH);
      lcd.println("Welcome to the BMS\n" + lcdText);
      break;
    }
    case CHARGE:
    {
      digitalWrite(chargeRelay, LOW);
      digitalWrite(dischargeRelay, HIGH);
      lcd.println("Charging...\n" + lcdText + "\nCurrent: 600mA");
      break;
    }
    case FAST_CHARGE:
    {
      digitalWrite(chargeRelay, LOW);
      digitalWrite(dischargeRelay, HIGH);
      lcd.println("Fast Charging...\n" + lcdText + "\nCurrent: 1A");
      break;
    }
    case DISCHARGE:
    {
      digitalWrite(chargeRelay, HIGH);
      digitalWrite(dischargeRelay, LOW);
      lcd.println("Discharge...\n" + lcdText + "\nCurrent: 800mA");
      break;
    }
    }

    digitalWrite(fullIndicator, LOW);
    bool isCharging = state == CHARGE || state == FAST_CHARGE;
    if (batteryVoltage > 8.20)
    {
      if (isCharging)
      {
        state = OFF;
      }
      digitalWrite(fullIndicator, HIGH);
    }

    digitalWrite(chargeIndicator, isCharging);
    digitalWrite(fan, LOW);

    if (temp >= 50)
    {
      state = OFF;
      digitalWrite(fan, HIGH);
      lcd.println("Overheat\nStoping all");
      buzzer.errorBeep();
    }

    if (isShortCircuit)
    {
      state = OFF;
      lcd.println("Short circuit\nStoping all");
      buzzer.errorBeep();
    }

    if (inputVoltage > 12.0)
    {
      state = OFF;
      lcd.println("Over voltage\nStoping all");
      buzzer.errorBeep();
    }
    delay(100);
  }
}

void loop()
{
}

const float R2 = 30000.0, R1 = 7500.0, refVoltage = 5.0;
float readBatteryVoltage()
{
  int refValue = analogRead(A0);
  float tempVoltage = (refValue * refVoltage) / 1024.0;
  return tempVoltage / (R1 / (R1 + R2));
}

const float sensitivity = 0.185, vRef = 2.5, calibrationFactor = 5.0 / 1023.0;
float readCurrent()
{
  int rawAnalogValue = analogRead(A1);
  float voltage = rawAnalogValue * calibrationFactor;
  return (voltage - vRef) / sensitivity;
}

float readInputVoltage(uint8_t pin)
{
  int ref = analogRead(pin);
  float value = map(ref, 0, 1024, 800, 1500);
  return value / 100.0;
}