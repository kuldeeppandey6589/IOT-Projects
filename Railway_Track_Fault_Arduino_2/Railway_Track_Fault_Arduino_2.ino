#include <SoftwareSerial.h>

// ======================================
// IR SENSORS
// ======================================

#define IR_LEFT  2
#define IR_RIGHT 3

// ======================================
// ULTRASONIC
// ======================================

#define TRIG_PIN 4
#define ECHO_PIN 5

// ======================================
// COMMUNICATION WITH ARDUINO #1
//
// D8 = TX
// D9 = RX
// ======================================

SoftwareSerial Arduino1(9, 8);

// ======================================
// SIM800L
//
// D10 = RX
// D11 = TX
// ======================================

SoftwareSerial SIM800(10, 11);

// ======================================
// PHONE NUMBER
// ======================================

String phoneNumber = "+918960383086";

// ======================================
// SETTINGS
// ======================================

#define OBJECT_DISTANCE 20

// ======================================
// STATUS
// ======================================

bool objectDetected = false;
bool objectSMSsent = false;

bool pathFault = false;
bool faultSMSsent = false;

// ======================================
// SETUP
// ======================================

void setup() {

  Serial.begin(9600);

  Arduino1.begin(9600);
  SIM800.begin(9600);

  // IR
  pinMode(IR_LEFT, INPUT);
  pinMode(IR_RIGHT, INPUT);

  // Ultrasonic
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  delay(2000);

  // ====================================
  // SIM800L INITIALIZATION
  // ====================================

  SIM800.listen();

  SIM800.println("AT");
  delay(1000);

  SIM800.println("ATE0");
  delay(1000);

  SIM800.println("AT+CMGF=1");
  delay(1000);

  // Switch back to Arduino communication
  Arduino1.listen();

  Serial.println("====================");
  Serial.println("ARDUINO #2 READY");
  Serial.println("====================");
}

// ======================================
// LOOP
// ======================================

void loop() {

  // ====================================
  // 1. CHECK ULTRASONIC FIRST
  // ====================================

  long distance = getDistance();

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  // ====================================
  // OBJECT DETECTED
  // ====================================

  if (distance > 0 && distance <= OBJECT_DISTANCE) {

    objectDetected = true;

    Serial.println("--------------------");
    Serial.println("OBJECT DETECTED");
    Serial.println("CAR STOP");
    Serial.println("--------------------");

    // Tell Arduino #1 to stop
    sendToArduino1("OBSTACLE");

    // ==================================
    // SEND OBJECT SMS ONLY ONCE
    // ==================================

    if (!objectSMSsent) {

      objectSMSsent = true;

      bool result = sendSMS(
        "ALERT! Object detected in front of robotic car. Car stopped."
      );

      if (result) {

        // Tell Arduino #1 SMS was sent
        sendToArduino1("SMS_SENT");

      } else {

        // Tell Arduino #1 SMS failed
        sendToArduino1("SMS_FAILED");
      }
    }

    delay(200);

    return;
  }

  // ====================================
  // OBJECT REMOVED
  // ====================================

  if (objectDetected) {

    objectDetected = false;
    objectSMSsent = false;

    Serial.println("--------------------");
    Serial.println("OBJECT REMOVED");
    Serial.println("--------------------");

    sendToArduino1("CLEAR");

    delay(200);
  }

  // ====================================
  // 2. READ IR SENSORS
  // ====================================

  int leftIR = digitalRead(IR_LEFT);
  int rightIR = digitalRead(IR_RIGHT);

  Serial.print("LEFT IR: ");
  Serial.print(leftIR);

  Serial.print(" | RIGHT IR: ");
  Serial.println(rightIR);

  // ====================================
  // BOTH IR LOW = PATH CLEAR
  // ====================================

  if (leftIR == LOW && rightIR == LOW) {

    pathFault = false;
    faultSMSsent = false;

    Serial.println("PATH CLEAR");

    sendToArduino1("PATH_CLEAR");
  }

  // ====================================
  // ANY IR HIGH = PATH FAULT
  // ====================================

  else {

    pathFault = true;

    Serial.println("PATH FAULT");

    sendToArduino1("PATH_FAULT");

    // ==================================
    // SEND FAULT SMS ONLY ONCE
    // ==================================

    if (!faultSMSsent) {

      faultSMSsent = true;

      bool result = sendSMS(
        "ALERT! Robotic car detected a fault on the track. Car stopped."
      );

      if (result) {

        sendToArduino1("SMS_SENT");

      } else {

        sendToArduino1("SMS_FAILED");
      }
    }
  }

  delay(300);
}

// ======================================
// SEND MESSAGE TO ARDUINO #1
// ======================================

void sendToArduino1(String message) {

  Arduino1.listen();

  delay(5);

  Arduino1.println(message);

  Arduino1.flush();

  Serial.print("TO ARDUINO #1: ");
  Serial.println(message);
}

// ======================================
// ULTRASONIC
// ======================================

long getDistance() {

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(
    ECHO_PIN,
    HIGH,
    30000
  );

  if (duration == 0) {
    return -1;
  }

  long distance = duration * 0.034 / 2;

  return distance;
}

// ======================================
// SEND SMS
// ======================================

bool sendSMS(String message) {

  Serial.println("SENDING SMS...");

  // Switch to SIM800L
  SIM800.listen();

  delay(100);

  // Text mode
  SIM800.println("AT+CMGF=1");
  delay(1000);

  // Phone number
  SIM800.print("AT+CMGS=\"");
  SIM800.print(phoneNumber);
  SIM800.println("\"");

  delay(1500);

  // Message
  SIM800.print(message);

  delay(500);

  // CTRL + Z
  SIM800.write(26);

  // Wait for SMS transmission
  delay(5000);

  Serial.println("SMS COMMAND SENT");

  // Switch back to Arduino #1
  Arduino1.listen();

  delay(100);

  return true;
}