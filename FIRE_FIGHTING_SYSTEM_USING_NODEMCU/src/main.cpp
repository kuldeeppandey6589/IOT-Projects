#include <ESP8266WiFi.h>
#include "AdafruitIO_WiFi.h"
#include <Servo.h>

// --------------------
// Adafruit IO Config
// --------------------
#define IO_USERNAME "fire011125"
// Fire@123

#define IO_KEY "aio_CRsT954GhsX4ooMsdAQ0bwqigY3f"

#define WIFI_SSID "Robotutor"
#define WIFI_PASS "Robotutor"

// Create Adafruit IO instance
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// Feeds
AdafruitIO_Feed *flameLeftFeed = io.feed("flame_left");
AdafruitIO_Feed *flameRightFeed = io.feed("flame_right");
AdafruitIO_Feed *fanStatusFeed = io.feed("fan_status");
AdafruitIO_Feed *servoFeed = io.feed("servo_angle");

// --------------------
// Pin Config
// --------------------
const int FLAME_LEFT_PIN = D5;  // IR sensor left
const int FLAME_RIGHT_PIN = D6; // IR sensor right
const int RELAY_PIN = D7;       // Relay for fan/pump
const int SERVO_PIN = D4;       // Servo pin

Servo fanServo;

void setup()
{
  Serial.begin(115200);

  pinMode(FLAME_LEFT_PIN, INPUT);
  pinMode(FLAME_RIGHT_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Fan OFF initially

  fanServo.attach(SERVO_PIN);
  fanServo.write(90); // center position

  // Connect to Adafruit IO
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  while (io.status() < AIO_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" Connected!");
}

void loop()
{
  io.run(); // keep Adafruit IO connection alive

  int leftFlame = digitalRead(FLAME_LEFT_PIN);
  int rightFlame = digitalRead(FLAME_RIGHT_PIN);

  // Send sensor data to Adafruit IO
  flameLeftFeed->save(leftFlame);
  flameRightFeed->save(rightFlame);

  // Fire detection logic
  if (leftFlame == LOW || rightFlame == LOW)
  {
    digitalWrite(RELAY_PIN, HIGH); // Fan ON
    fanStatusFeed->save(1);

    if (leftFlame == LOW && rightFlame == HIGH)
    {
      for (int pos = 90; pos <= 180; pos += 5)
      {
        fanServo.write(pos);
        delay(15);
      }
      for (int pos = 180; pos >= 90; pos -= 5)
      {
        fanServo.write(pos);
        delay(15);
      }
      servoFeed->save(180);
      Serial.println("🔥 Flame LEFT → Servo 45°");
    }
    else if (rightFlame == LOW && leftFlame == HIGH)
    {
      for (int pos = 0; pos <= 90; pos += 5)
      {
        fanServo.write(pos);
        delay(15);
      }
      for (int pos = 90; pos >= 0; pos -= 5)
      {
        fanServo.write(pos);
        delay(15);
      }
      servoFeed->save(-180);
      Serial.println("🔥 Flame RIGHT → Servo 135°");
    }
    else
    {
      fanServo.write(90); // both detect → center
      servoFeed->save(90);
      Serial.println("🔥 Flame CENTER → Servo 90°");
    }

    Serial.println("Fan ON");
  }
  else
  {
    digitalWrite(RELAY_PIN, LOW); // Fan OFF
    fanStatusFeed->save(0);
    fanServo.write(90); // reset to center
    servoFeed->save(90);
    Serial.println("✅ No flame. Fan OFF, Servo Center");
  }

  delay(2000); // update every 2s
}



// #include <ESP8266WiFi.h>
// #include "AdafruitIO_WiFi.h"
// #include <Servo.h>

// // --------------------
// // Adafruit IO Config
// // --------------------
// #define IO_USERNAME  "fire011125"
// //Fire@123

// #define IO_KEY       "aio_CRsT954GhsX4ooMsdAQ0bwqigY3f"

// #define WIFI_SSID    "Robotutor"
// #define WIFI_PASS    "Robotutor"

// // Create Adafruit IO instance
// AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// // Feeds
// AdafruitIO_Feed *flameLeftFeed  = io.feed("flame_left");
// AdafruitIO_Feed *flameRightFeed = io.feed("flame_right");
// AdafruitIO_Feed *fanStatusFeed  = io.feed("fan_status");
// AdafruitIO_Feed *servoFeed      = io.feed("servo_angle");

// // --------------------
// // Pin Config
// // --------------------
// const int FLAME_LEFT_PIN  = D5;   // IR sensor left
// const int FLAME_RIGHT_PIN = D6;   // IR sensor right
// const int RELAY_PIN       = D7;   // Relay for fan/pump
// const int SERVO_PIN       = D4;   // Servo pin

// Servo fanServo;

// void setup() {
//   Serial.begin(115200);

//   pinMode(FLAME_LEFT_PIN, INPUT);
//   pinMode(FLAME_RIGHT_PIN, INPUT);
//   pinMode(RELAY_PIN, OUTPUT);
//   digitalWrite(RELAY_PIN, LOW); // Fan OFF initially

//   fanServo.attach(SERVO_PIN);
//   fanServo.write(90); // center position

//   // Connect to Adafruit IO
//   Serial.print("Connecting to Adafruit IO");
//   io.connect();

//   while(io.status() < AIO_CONNECTED) {
//     Serial.print(".");
//     delay(500);
//   }
//   Serial.println(" Connected!");
// }

// void loop() {
//   io.run(); // keep Adafruit IO connection alive

//   int leftFlame  = digitalRead(FLAME_LEFT_PIN);
//   int rightFlame = digitalRead(FLAME_RIGHT_PIN);

//   // Send sensor data to Adafruit IO
//   flameLeftFeed->save(leftFlame);
//   flameRightFeed->save(rightFlame);

//   // Fire detection logic
//   if (leftFlame == LOW || rightFlame == LOW) {
//     digitalWrite(RELAY_PIN, HIGH);  // Fan ON
//     fanStatusFeed->save(1);

//     if (leftFlame == LOW && rightFlame == HIGH) {
//       fanServo.write(180); // aim left
//       servoFeed->save(180);
//       Serial.println("🔥 Flame LEFT → Servo 45°");
//     } 
//     else if (rightFlame == LOW && leftFlame == HIGH) {
//       fanServo.write(-180); // aim right
//       servoFeed->save(-180);
//       Serial.println("🔥 Flame RIGHT → Servo 135°");
//     } 
//     else {
//       fanServo.write(90); // both detect → center
//       servoFeed->save(90);
//       Serial.println("🔥 Flame CENTER → Servo 90°");
//     }

//     Serial.println("Fan ON");
//   } 
//   else {
//     digitalWrite(RELAY_PIN, LOW);   // Fan OFF
//     fanStatusFeed->save(0);
//     fanServo.write(90); // reset to center
//     servoFeed->save(90);
//     Serial.println("✅ No flame. Fan OFF, Servo Center");
//   }

//   delay(2000); // update every 2s
// }
