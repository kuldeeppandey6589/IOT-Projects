#include <NewPing.h>
#include <Servo.h>

// Motor pins
const int LeftMotorForward = 9;
const int LeftMotorBackward = 10;
const int RightMotorForward = 11;
const int RightMotorBackward = 12;

// ESP32-CAM trigger pin
const int EspCamTriggerPin = 7;

// Ultrasonic sensor pins
#define trig_pin A1
#define echo_pin A2

#define maximum_distance 200

boolean goesForward = false;
int distance = 100;

NewPing sonar(trig_pin, echo_pin, maximum_distance);
Servo servo_motor;

// Photo trigger control
unsigned long lastPhotoTriggerTime = 0;
const unsigned long photoCooldown = 10000; // 10 seconds gap between photos

void setup() {
  Serial.begin(9600);
  Serial.println("Obstacle Avoiding Car Started");

  pinMode(RightMotorForward, OUTPUT);
  pinMode(LeftMotorForward, OUTPUT);
  pinMode(LeftMotorBackward, OUTPUT);
  pinMode(RightMotorBackward, OUTPUT);

  pinMode(EspCamTriggerPin, OUTPUT);
  digitalWrite(EspCamTriggerPin, LOW);

  servo_motor.attach(8);

  servo_motor.write(115);
  delay(2000);

  distance = readPing();
  delay(100);
  distance = readPing();
  delay(100);
  distance = readPing();
  delay(100);
  distance = readPing();
  delay(100);
}

void loop() {
  int distanceRight = 0;
  int distanceLeft = 0;

  delay(50);

  Serial.print("Front Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance <= 20) {
    Serial.println("Obstacle detected!");
    triggerEspCam();

    moveStop();
    delay(300);

    moveBackward();
    delay(400);

    moveStop();
    delay(300);

    distanceRight = lookRight();
    delay(300);

    distanceLeft = lookLeft();
    delay(300);

    Serial.print("Right Distance: ");
    Serial.print(distanceRight);
    Serial.println(" cm");

    Serial.print("Left Distance: ");
    Serial.print(distanceLeft);
    Serial.println(" cm");

    if (distanceRight >= distanceLeft) {
      Serial.println("Turning Right");
      turnRight();
      moveStop();
    } else {
      Serial.println("Turning Left");
      turnLeft();
      moveStop();
    }
  } else {
    Serial.println("Moving Forward");
    moveForward();
  }

  distance = readPing();
}

void triggerEspCam() {
  if (millis() - lastPhotoTriggerTime >= photoCooldown) {
    lastPhotoTriggerTime = millis();

    Serial.println("Trigger sent to ESP32-CAM");

    digitalWrite(EspCamTriggerPin, HIGH);
    delay(1000);
    digitalWrite(EspCamTriggerPin, LOW);
  }
}

int lookRight() {
  servo_motor.write(50);
  delay(500);

  int distance = readPing();
  delay(100);

  servo_motor.write(115);
  return distance;
}

int lookLeft() {
  servo_motor.write(170);
  delay(500);

  int distance = readPing();
  delay(100);

  servo_motor.write(115);
  return distance;
}

int readPing() {
  delay(70);

  int cm = sonar.ping_cm();

  if (cm == 0) {
    cm = 250;
  }

  return cm;
}

void moveStop() {
  digitalWrite(RightMotorForward, LOW);
  digitalWrite(LeftMotorForward, LOW);
  digitalWrite(RightMotorBackward, LOW);
  digitalWrite(LeftMotorBackward, LOW);

  Serial.println("Motor Stop");
}

void moveForward() {
  if (!goesForward) {
    goesForward = true;

    digitalWrite(LeftMotorForward, HIGH);
    digitalWrite(RightMotorForward, HIGH);

    digitalWrite(LeftMotorBackward, LOW);
    digitalWrite(RightMotorBackward, LOW);

    Serial.println("Motor Forward");
  }
}

void moveBackward() {
  goesForward = false;

  digitalWrite(LeftMotorBackward, HIGH);
  digitalWrite(RightMotorBackward, HIGH);

  digitalWrite(LeftMotorForward, LOW);
  digitalWrite(RightMotorForward, LOW);

  Serial.println("Motor Backward");
}

void turnRight() {
  digitalWrite(LeftMotorForward, HIGH);
  digitalWrite(RightMotorBackward, HIGH);

  digitalWrite(LeftMotorBackward, LOW);
  digitalWrite(RightMotorForward, LOW);

  delay(500);

  digitalWrite(LeftMotorForward, HIGH);
  digitalWrite(RightMotorForward, HIGH);

  digitalWrite(LeftMotorBackward, LOW);
  digitalWrite(RightMotorBackward, LOW);
}

void turnLeft() {
  digitalWrite(LeftMotorBackward, HIGH);
  digitalWrite(RightMotorForward, HIGH);

  digitalWrite(LeftMotorForward, LOW);
  digitalWrite(RightMotorBackward, LOW);

  delay(500);

  digitalWrite(LeftMotorForward, HIGH);
  digitalWrite(RightMotorForward, HIGH);

  digitalWrite(LeftMotorBackward, LOW);
  digitalWrite(RightMotorBackward, LOW);
}