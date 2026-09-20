/***************************************************
   ESP32 WiFi Robotic Car + Voice (HC-05)
   + Ultrasonic Auto Stop
****************************************************/

#include <WiFi.h>
#include <WebServer.h>

// ------------- USER CONFIG -----------------
const char* ssid = "homeiot";
const char* password = "homeiot123";
// -------------------------------------------

// --------- BLUETOOTH (HC-05) CONFIG ---------
#define BT_RX 16
#define BT_TX 17
HardwareSerial BT(2);

// --------- ULTRASONIC CONFIG ----------
#define TRIG_PIN 5
#define ECHO_PIN 18
#define OBSTACLE_DISTANCE_CM 20

// --------- MOTOR DRIVER PINS ----------
const int IN1 = 26;
const int IN2 = 27;
const int IN3 = 14;
const int IN4 = 12;

WebServer server(80);

String carState = "stop";

// ---------- MOTOR CONTROL ----------
void stopCar() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  carState = "stop";
  Serial.println("STOP");
}

void forward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  carState = "forward";
  Serial.println("FORWARD");
}

void backward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  carState = "backward";
  Serial.println("BACKWARD");
}

void left() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  carState = "left";
  Serial.println("LEFT");
}

void right() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  carState = "right";
  Serial.println("RIGHT");
}

// ---------- ULTRASONIC ----------
long readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) return -1;

  return duration * 0.034 / 2;
}

bool isObstacleDetected() {
  long d = readDistanceCM();
  Serial.print("Distance: ");
  Serial.println(d);

  if (d > 0 && d <= OBSTACLE_DISTANCE_CM) {
    return true;
  }
  return false;
}

// ---------- AUTO STOP ----------
void checkObstacleAndStop() {
  if (carState == "forward" || carState == "left" || carState == "right" || carState == "backward") {
    if (isObstacleDetected()) {
      Serial.println("Obstacle detected -> STOP");
      stopCar();
    }
  }
}

// ---------- BLUETOOTH ----------
void handleBluetooth() {
  if (!BT.available()) return;

  String cmd = BT.readStringUntil('\n');
  cmd.trim();
  cmd.toLowerCase();

  if (cmd.length() == 0) return;

  Serial.print("BT cmd: ");
  Serial.println(cmd);

  if (cmd == "f" || cmd.indexOf("forward") >= 0) {
    if (isObstacleDetected()) stopCar();
    else forward();
  }
  else if (cmd == "b" || cmd.indexOf("back") >= 0 || cmd.indexOf("backward") >= 0) {
    if (isObstacleDetected()) stopCar();
    else backward();
  }
  else if (cmd == "l" || cmd.indexOf("left") >= 0) {
    if (isObstacleDetected()) stopCar();
    else left();
  }
  else if (cmd == "r" || cmd.indexOf("right") >= 0) {
    if (isObstacleDetected()) stopCar();
    else right();
  }
  else if (cmd == "s" || cmd.indexOf("stop") >= 0) {
    stopCar();
  }
}

// ---------- WEB PAGE ----------
String htmlPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Car</title>
<style>
body { background:#111; color:white; text-align:center; font-family:Arial; }
button { width:120px; height:60px; margin:10px; font-size:18px; }
</style>
</head>
<body>

<h2>ESP32 Car Control</h2>

<button onclick="sendCmd('forward')">Forward</button><br>
<button onclick="sendCmd('left')">Left</button>
<button onclick="sendCmd('right')">Right</button><br>
<button onclick="sendCmd('backward')">Backward</button><br>
<button onclick="sendCmd('stop')">STOP</button>

<script>
function sendCmd(cmd){
  fetch('/' + cmd);
}
</script>

</body>
</html>
)rawliteral";
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  stopCar();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  BT.begin(9600, SERIAL_8N1, BT_RX, BT_TX);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", []() {
    server.send(200, "text/html", htmlPage());
  });

  server.on("/forward", []() {
    if (isObstacleDetected()) {
      stopCar();
      server.send(200, "text/plain", "Forward blocked - obstacle");
    } else {
      forward();
      server.send(200, "text/plain", "Forward");
    }
  });

  server.on("/backward", []() {
    if (isObstacleDetected()) {
      stopCar();
      server.send(200, "text/plain", "Backward blocked - obstacle");
    } else {
      backward();
      server.send(200, "text/plain", "Backward");
    }
  });

  server.on("/left", []() {
    if (isObstacleDetected()) {
      stopCar();
      server.send(200, "text/plain", "Left blocked - obstacle");
    } else {
      left();
      server.send(200, "text/plain", "Left");
    }
  });

  server.on("/right", []() {
    if (isObstacleDetected()) {
      stopCar();
      server.send(200, "text/plain", "Right blocked - obstacle");
    } else {
      right();
      server.send(200, "text/plain", "Right");
    }
  });

  server.on("/stop", []() {
    stopCar();
    server.send(200, "text/plain", "Stop");
  });

  server.begin();
  Serial.println("Server started");
}

// ---------- LOOP ----------
void loop() {
  server.handleClient();
  handleBluetooth();
  checkObstacleAndStop();
  delay(50);
}