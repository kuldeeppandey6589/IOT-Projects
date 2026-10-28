#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>

// =======================
// WiFi Details
// =======================
const char* ssid = "homeiot";
const char* password = "homeiot123";

// =======================
// SIM800L GSM
// =======================
// SIM800L TX -> ESP32 GPIO16
// SIM800L RX -> ESP32 GPIO17
#define SIM800_RX 16
#define SIM800_TX 17

HardwareSerial sim800(1);

// Change this mobile number
String phoneNumber = "+917985599292";

// SMS alert control
bool smsSent = false;

// =======================
// Ultrasonic Sensor 1 - Left Side
// =======================
#define TRIG_LEFT   4
#define ECHO_LEFT   2

// =======================
// Ultrasonic Sensor 2 - Right Side
// =======================
#define TRIG_RIGHT  18
#define ECHO_RIGHT  19

// =======================
// Flame, Relay, Buzzer
// =======================
#define FLAME_PIN    34
#define RELAY_PIN    26
#define BUZZER_PIN   27

// =======================
// LED Pins
// =======================
#define LEFT_GREEN_LED   13
#define LEFT_RED_LED     12

#define RIGHT_GREEN_LED  14
#define RIGHT_RED_LED    25

// =======================
// Settings
// =======================
#define OBJECT_DISTANCE_CM 30
#define FLAME_THRESHOLD    1000

// Most relay modules are ACTIVE LOW
#define RELAY_ON   LOW
#define RELAY_OFF  HIGH

WebServer server(80);

// =======================
// Status Variables
// =======================
bool leftObjectDetected = false;
bool rightObjectDetected = false;
bool objectDetected = false;
bool bothObjectDetected = false;
bool fireDetected = false;
bool relayState = false;
bool manualRelay = false;

long leftDistance = 999;
long rightDistance = 999;

// =======================
// Read SIM800L Response
// =======================
String readSIM800(unsigned long timeout) {
  String response = "";
  unsigned long startTime = millis();

  while (millis() - startTime < timeout) {
    while (sim800.available()) {
      char c = sim800.read();
      response += c;
      Serial.write(c);
    }
  }

  Serial.println();
  return response;
}

// =======================
// Send AT Command
// =======================
bool sendATCommand(String command, String expected, unsigned long timeout) {
  Serial.print("CMD: ");
  Serial.println(command);

  sim800.println(command);

  String response = readSIM800(timeout);

  if (response.indexOf(expected) != -1) {
    Serial.println("OK RESPONSE FOUND");
    return true;
  } else {
    Serial.println("EXPECTED RESPONSE NOT FOUND");
    return false;
  }
}

// =======================
// Send SMS Function
// =======================
void sendSMS(String message) {
  Serial.println("================================");
  Serial.println("SMS SENDING STARTED");
  Serial.print("Mobile Number: ");
  Serial.println(phoneNumber);
  Serial.println("Message:");
  Serial.println(message);
  Serial.println("================================");

  // Test SIM800L
  if (!sendATCommand("AT", "OK", 3000)) {
    Serial.println("ERROR: SIM800L not responding.");
    Serial.println("Check TX/RX wiring and power supply.");
    return;
  }

  // Check SIM card
  if (!sendATCommand("AT+CPIN?", "READY", 3000)) {
    Serial.println("ERROR: SIM card not ready.");
    Serial.println("Check SIM card or disable SIM PIN lock.");
    return;
  }

  // Check network registration
  Serial.println("Checking network registration...");
  sim800.println("AT+CREG?");
  String networkResponse = readSIM800(3000);

  if (networkResponse.indexOf("+CREG: 0,1") != -1 || networkResponse.indexOf("+CREG: 0,5") != -1) {
    Serial.println("Network registered.");
  } else {
    Serial.println("ERROR: Network not registered.");
    Serial.println("Check antenna, signal, SIM network, and power supply.");
    return;
  }

  // Check signal
  sendATCommand("AT+CSQ", "OK", 3000);

  // SMS text mode
  if (!sendATCommand("AT+CMGF=1", "OK", 3000)) {
    Serial.println("ERROR: SMS text mode failed.");
    return;
  }

  // GSM character set
  sendATCommand("AT+CSCS=\"GSM\"", "OK", 3000);

  Serial.println("Sending phone number...");

  sim800.print("AT+CMGS=\"");
  sim800.print(phoneNumber);
  sim800.println("\"");

  String response = readSIM800(5000);

  if (response.indexOf(">") == -1) {
    Serial.println("ERROR: SIM800L did not give > prompt.");
    Serial.println("SMS cannot continue.");
    return;
  }

  Serial.println("Typing SMS message...");
  sim800.print(message);
  delay(500);

  Serial.println("Sending CTRL+Z...");
  sim800.write(26);

  response = readSIM800(15000);

  if (response.indexOf("+CMGS") != -1 && response.indexOf("OK") != -1) {
    Serial.println("SMS SENT SUCCESSFULLY");
  } else {
    Serial.println("SMS FAILED");
    Serial.println("Check balance, SIM network, phone number, and antenna.");
  }

  Serial.println("================================");
}

// =======================
// Alert Message
// =======================
void sendAlertMessage() {
  String msg = "ALERT from ESP32!\n";

  if (bothObjectDetected) {
    msg += "Object detected on BOTH sides.\n";
  }

  if (fireDetected) {
    msg += "Fire detected.\n";
  }

  msg += "Left Distance: ";
  msg += String(leftDistance);
  msg += " cm\n";

  msg += "Right Distance: ";
  msg += String(rightDistance);
  msg += " cm\n";

  sendSMS(msg);
}

// =======================
// Ultrasonic Distance Function
// =======================
long getDistanceCM(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);

  if (duration == 0) {
    return 999;
  }

  long distance = duration * 0.034 / 2;
  return distance;
}

// =======================
// LED Logic
// =======================
void updateLEDs() {
  // Normal condition: no object on both sides
  if (!leftObjectDetected && !rightObjectDetected) {
    digitalWrite(LEFT_GREEN_LED, LOW);
    digitalWrite(LEFT_RED_LED, LOW);
    digitalWrite(RIGHT_GREEN_LED, LOW);
    digitalWrite(RIGHT_RED_LED, LOW);
  }

  // Both ultrasonic sensors detect object
  else if (leftObjectDetected && rightObjectDetected) {
    digitalWrite(LEFT_GREEN_LED, LOW);
    digitalWrite(LEFT_RED_LED, HIGH);

    digitalWrite(RIGHT_GREEN_LED, LOW);
    digitalWrite(RIGHT_RED_LED, HIGH);
  }

  // Only left side detects object
  else if (leftObjectDetected && !rightObjectDetected) {
    digitalWrite(LEFT_GREEN_LED, LOW);
    digitalWrite(LEFT_RED_LED, HIGH);

    digitalWrite(RIGHT_GREEN_LED, HIGH);
    digitalWrite(RIGHT_RED_LED, LOW);
  }

  // Only right side detects object
  else if (!leftObjectDetected && rightObjectDetected) {
    digitalWrite(LEFT_GREEN_LED, HIGH);
    digitalWrite(LEFT_RED_LED, LOW);

    digitalWrite(RIGHT_GREEN_LED, LOW);
    digitalWrite(RIGHT_RED_LED, HIGH);
  }
}

// =======================
// Sensor Reading
// =======================
void readSensors() {
  leftDistance = getDistanceCM(TRIG_LEFT, ECHO_LEFT);
  rightDistance = getDistanceCM(TRIG_RIGHT, ECHO_RIGHT);

  leftObjectDetected = leftDistance <= OBJECT_DISTANCE_CM;
  rightObjectDetected = rightDistance <= OBJECT_DISTANCE_CM;

  objectDetected = leftObjectDetected || rightObjectDetected;
  bothObjectDetected = leftObjectDetected && rightObjectDetected;

  int flameValue = analogRead(FLAME_PIN);

  // Many flame sensors give LOW value when flame is detected
  if (flameValue < FLAME_THRESHOLD) {
    fireDetected = true;
  } else {
    fireDetected = false;
  }

  updateLEDs();

  // Buzzer logic
  if (objectDetected || fireDetected) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Relay logic
  if (fireDetected || manualRelay) {
    relayState = true;
    digitalWrite(RELAY_PIN, RELAY_ON);
  } else {
    relayState = false;
    digitalWrite(RELAY_PIN, RELAY_OFF);
  }

  // =======================
  // SMS Logic
  // SMS sends only when:
  // both side object detected OR fire detected
  // =======================
  if (bothObjectDetected || fireDetected) {
    if (!smsSent) {
      Serial.println("ALERT CONDITION DETECTED");

      if (bothObjectDetected) {
        Serial.println("Reason: Both side object detected");
      }

      if (fireDetected) {
        Serial.println("Reason: Fire detected");
      }

      sendAlertMessage();
      smsSent = true;
    }
  } else {
    smsSent = false;
  }

  // Serial.print("Left Distance: ");
  // Serial.print(leftDistance);
  // Serial.print(" cm | Right Distance: ");
  // Serial.print(rightDistance);
  // Serial.print(" cm | Both Object: ");
  // Serial.print(bothObjectDetected ? "YES" : "NO");
  // Serial.print(" | Fire: ");
  // Serial.print(fireDetected ? "YES" : "NO");
  // Serial.print(" | SMS Sent: ");
  // Serial.println(smsSent ? "YES" : "NO");
}

// =======================
// Webpage
// =======================
String webpage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Accident Detection System</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <style>
    body {
      font-family: Arial, sans-serif;
      background: #f2f2f2;
      text-align: center;
      margin: 0;
      padding: 20px;
    }

    .container {
      max-width: 500px;
      margin: auto;
      background: white;
      padding: 20px;
      border-radius: 15px;
      box-shadow: 0 0 15px rgba(0,0,0,0.2);
    }

    h1 {
      color: #222;
      font-size: 24px;
    }

    .card {
      background: #eeeeee;
      margin: 15px 0;
      padding: 15px;
      border-radius: 10px;
      font-size: 20px;
    }

    .safe {
      color: green;
      font-weight: bold;
    }

    .danger {
      color: red;
      font-weight: bold;
    }

    button {
      padding: 12px 25px;
      margin: 10px;
      border: none;
      border-radius: 8px;
      font-size: 18px;
      cursor: pointer;
      color: white;
    }

    .on {
      background: green;
    }

    .off {
      background: red;
    }
  </style>
</head>

<body>
  <div class="container">
    <h1>ESP32 Accident Detection System</h1>

    <div class="card">
      Left Object: <span id="leftObject">Loading...</span>
    </div>

    <div class="card">
      Right Object: <span id="rightObject">Loading...</span>
    </div>

    <div class="card">
      Left Distance: <span id="leftDistance">Loading...</span>
    </div>

    <div class="card">
      Right Distance: <span id="rightDistance">Loading...</span>
    </div>

    <div class="card">
      Fire Status: <span id="fire">Loading...</span>
    </div>

    <div class="card">
      Relay Status: <span id="relay">Loading...</span>
    </div>

    <button class="on" onclick="relayOn()">Relay ON</button>
    <button class="off" onclick="relayOff()">Relay OFF</button>
  </div>

  <script>
    function updateData() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById("leftObject").innerHTML =
            data.leftObject ? "<span class='danger'>OBJECT DETECTED</span>" : "<span class='safe'>SAFE</span>";

          document.getElementById("rightObject").innerHTML =
            data.rightObject ? "<span class='danger'>OBJECT DETECTED</span>" : "<span class='safe'>SAFE</span>";

          document.getElementById("leftDistance").innerHTML = data.leftDistance + " cm";
          document.getElementById("rightDistance").innerHTML = data.rightDistance + " cm";

          document.getElementById("fire").innerHTML =
            data.fire ? "<span class='danger'>FIRE DETECTED</span>" : "<span class='safe'>NO FIRE</span>";

          document.getElementById("relay").innerHTML =
            data.relay ? "<span class='danger'>ON</span>" : "<span class='safe'>OFF</span>";
        });
    }

    function relayOn() {
      fetch('/relay/on');
    }

    function relayOff() {
      fetch('/relay/off');
    }

    setInterval(updateData, 1000);
    updateData();
  </script>
</body>
</html>
)rawliteral";

  return html;
}

// =======================
// Server Routes
// =======================
void handleRoot() {
  server.send(200, "text/html", webpage());
}

void handleData() {
  String json = "{";
  json += "\"leftObject\":" + String(leftObjectDetected ? "true" : "false") + ",";
  json += "\"rightObject\":" + String(rightObjectDetected ? "true" : "false") + ",";
  json += "\"leftDistance\":" + String(leftDistance) + ",";
  json += "\"rightDistance\":" + String(rightDistance) + ",";
  json += "\"fire\":" + String(fireDetected ? "true" : "false") + ",";
  json += "\"relay\":" + String(relayState ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}

void handleRelayOn() {
  manualRelay = true;
  server.send(200, "text/plain", "Relay ON");
}

void handleRelayOff() {
  manualRelay = false;
  server.send(200, "text/plain", "Relay OFF");
}

// =======================
// Setup
// =======================
void setup() {
  Serial.begin(115200);
  delay(1000);

  // SIM800L start
  sim800.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);
  delay(5000);

  Serial.println("Initializing SIM800L...");
  sendATCommand("AT", "OK", 3000);
  sendATCommand("AT+CPIN?", "READY", 3000);
  sendATCommand("AT+CREG?", "OK", 3000);
  sendATCommand("AT+CSQ", "OK", 3000);
  sendATCommand("AT+CMGF=1", "OK", 3000);

  pinMode(TRIG_LEFT, OUTPUT);
  pinMode(ECHO_LEFT, INPUT);

  pinMode(TRIG_RIGHT, OUTPUT);
  pinMode(ECHO_RIGHT, INPUT);

  pinMode(FLAME_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(LEFT_GREEN_LED, OUTPUT);
  pinMode(LEFT_RED_LED, OUTPUT);
  pinMode(RIGHT_GREEN_LED, OUTPUT);
  pinMode(RIGHT_RED_LED, OUTPUT);

  digitalWrite(RELAY_PIN, RELAY_OFF);
  digitalWrite(BUZZER_PIN, LOW);

  digitalWrite(LEFT_GREEN_LED, LOW);
  digitalWrite(LEFT_RED_LED, LOW);
  digitalWrite(RIGHT_GREEN_LED, LOW);
  digitalWrite(RIGHT_RED_LED, LOW);

  WiFi.begin(ssid, password);

  Serial.println("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Connected!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/relay/on", handleRelayOn);
  server.on("/relay/off", handleRelayOff);

  server.begin();
  Serial.println("Web Server Started");
}

// =======================



   
// Loop
// =======================
void loop() {
  readSensors();
  server.handleClient();

  delay(100);
}