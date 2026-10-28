#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// LCD address: usually 0x27
LiquidCrystal_I2C lcd(0x27, 16, 2);

Servo myServo;

// Pin definitions
#define MQ2_PIN A0
#define FLAME_PIN 2
#define RELAY_PIN 7
#define BUZZER_PIN 8
#define SERVO_PIN 9

// Gas detection level
int gasThreshold = 400;

void setup() {
  Serial.begin(9600);

  pinMode(MQ2_PIN, INPUT);
  pinMode(FLAME_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  myServo.attach(SERVO_PIN);
  myServo.write(0);

  // Relay OFF
  digitalWrite(RELAY_PIN, HIGH);

  // Buzzer OFF
  digitalWrite(BUZZER_PIN, LOW);

  // LCD
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Gas & Flame");
  lcd.setCursor(0, 1);
  lcd.print("Detector");
  delay(2000);

  lcd.clear();
}

void loop() {

  int gasValue = analogRead(MQ2_PIN);

  // Most flame sensors output LOW when flame is detected
  int flameValue = digitalRead(FLAME_PIN);

  bool gasDetected = gasValue > gasThreshold;
  bool flameDetected = (flameValue == LOW);

  Serial.print("Gas: ");
  Serial.print(gasValue);

  Serial.print(" | Flame: ");
  Serial.println(flameDetected ? "DETECTED" : "NORMAL");

  // =========================
  // GAS DETECTED
  // =========================
  if (gasDetected) {

    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(RELAY_PIN, LOW);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("GAS DETECTED!");
    lcd.setCursor(0, 1);
    lcd.print("Relay: ON");

    // Keep servo at 0 for gas
    myServo.write(0);
  }

  // =========================
  // FLAME DETECTED
  // =========================
  else if (flameDetected) {

    digitalWrite(BUZZER_PIN, HIGH);

    // Relay OFF
    digitalWrite(RELAY_PIN, HIGH);

    // Servo turns 90 degrees
    myServo.write(90);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("FLAME DETECTED!");
    lcd.setCursor(0, 1);
    lcd.print("Gate Open");

  }

  // =========================
  // NORMAL CONDITION
  // =========================
  else {

    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(RELAY_PIN, LOW);

    // Servo returns to 0
    myServo.write(0);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("System Normal");
    lcd.setCursor(0, 1);
    lcd.print("Gas:");
    lcd.print(gasValue);
  }

  delay(500);
}