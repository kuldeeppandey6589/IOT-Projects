#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

// ======================================
// LCD
// ======================================

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ======================================
// L298N
// ======================================

#define ENA 5
#define ENB 6

#define IN1 7
#define IN2 8
#define IN3 9
#define IN4 10

// ======================================
// Arduino #2 communication
//
// Arduino #1 D12 = RX
// Arduino #1 D11 = TX (not used)
// ======================================

SoftwareSerial Arduino2(12, 11);

// ======================================
// SYSTEM
// ======================================

bool obstacle = false;

void setup() {

  Serial.begin(9600);

  Arduino2.begin(9600);

  // Motor pins
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Make sure motor is stopped
  stopMotor();

  // LCD
  lcd.init();
  lcd.backlight();

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("ROBOTIC CAR");

  lcd.setCursor(0, 1);
  lcd.print("SYSTEM READY");

  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WAITING...");
}

void loop() {

  // ======================================
  // RECEIVE COMMAND FROM ARDUINO #2
  // ======================================

  if (Arduino2.available()) {

    String command = Arduino2.readStringUntil('\n');

    command.trim();

    Serial.print("Received: ");
    Serial.println(command);

    // ====================================
    // OBJECT DETECTED
    // ====================================

    if (command == "OBSTACLE") {

      obstacle = true;

      stopMotor();

      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("OBJECT DETECTED");

      lcd.setCursor(0, 1);
      lcd.print("CAR STOPPED");

      Serial.println("OBJECT DETECTED");
      Serial.println("MOTOR STOPPED");
    }

    // ====================================
    // OBJECT CLEAR
    // ====================================

    else if (command == "CLEAR") {

      obstacle = false;

      stopMotor();

      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("CHECKING PATH");

      Serial.println("OBJECT CLEAR");
    }

    // ====================================
    // PATH CLEAR
    // ====================================

    else if (command == "PATH_CLEAR") {

      if (!obstacle) {

        moveForward();

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("PATH CLEAR");

        lcd.setCursor(0, 1);
        lcd.print("CAR MOVING");

        Serial.println("CAR MOVING");
      }
    }

    // ====================================
    // PATH FAULT
    // ====================================

    else if (command == "PATH_FAULT") {

      stopMotor();

      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("PATH FAULT");

      lcd.setCursor(0, 1);
      lcd.print("CAR STOPPED");

      Serial.println("PATH FAULT");
      Serial.println("MOTOR STOPPED");
    }
  }

  // ======================================
  // SAFETY
  // ======================================

  if (obstacle) {

    stopMotor();
  }
}

// ======================================
// MOVE FORWARD
// ======================================

void moveForward() {

  analogWrite(ENA, 220);
  analogWrite(ENB, 220);

  // Left motor
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  // Right motor
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

// ======================================
// STOP MOTOR
// ======================================

void stopMotor() {

  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}