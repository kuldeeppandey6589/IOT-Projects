/***************************************************
   ESP32 WiFi Robotic Car + DHT11 + Voice (HC-05)

   - Control via browser (phone / laptop)
   - Web buttons: Forward, Backward, Left, Right, Stop
   - Shows Temperature & Humidity from DHT11
   - Voice control via Bluetooth HC-05
     Commands (from phone voice app):
       "forward", "backward", "left", "right", "stop"
       or single letters: F, B, L, R, S

   Board: ESP32 Dev Module
****************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include "DHT.h"


// ------------- USER CONFIG -----------------
const char* ssid = "homeiot";
const char* password = "homeiot123";
// -------------------------------------------

// --------- DHT11 CONFIG ----------
#define DHTPIN 4  // DHT11 data pin connected to GPIO 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

float temperature = 0;
float humidity = 0;

// --------- BLUETOOTH (HC-05) CONFIG ---------
// Use UART2 on ESP32
#define BT_RX 16       // ESP32 RX2 <- HC-05 TX
#define BT_TX 17       // ESP32 TX2 -> HC-05 RX
HardwareSerial BT(2);  // UART2

// Motor driver pins (change if needed)
const int IN1 = 26;  // Left motor
const int IN2 = 27;
const int IN3 = 14;  // Right motor
const int IN4 = 12;

WebServer server(80);

// ---------- MOTOR CONTROL FUNCTIONS ----------
void stopCar() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  Serial.println("STOP");
}

void forward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  Serial.println("FORWARD");
}

void backward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  Serial.println("BACKWARD");
}

void left() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  Serial.println("LEFT");
}

void right() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  Serial.println("RIGHT");
}

// ---------- DHT READING FUNCTION ----------
void readDHT() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();  // Celsius

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  humidity = h;
  temperature = t;
}

// ---------- BLUETOOTH HANDLER ----------
void handleBluetooth() {
  if (!BT.available()) return;

  String cmd = BT.readStringUntil('\n');
  cmd.trim();
  cmd.toLowerCase();

  if (cmd.length() == 0) return;

  Serial.print("BT cmd: ");
  Serial.println(cmd);

  if (cmd == "f" || cmd.indexOf("forward") >= 0) {
    forward();
  } else if (cmd == "b" || cmd.indexOf("back") >= 0 || cmd.indexOf("backward") >= 0) {
    backward();
  } else if (cmd == "l" || cmd.indexOf("left") >= 0) {
    left();
  } else if (cmd == "r" || cmd.indexOf("right") >= 0) {
    right();
  } else if (cmd == "s" || cmd.indexOf("stop") >= 0) {
    stopCar();
  }
}

// ---------- WEB PAGE ----------
String htmlPage(float t, float h) {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Car</title>
<style>
body {
  font-family: Arial;
  text-align: center;
  background: #111;
  color: white;
}
.btn {
  width: 120px;
  height: 60px;
  font-size: 18px;
  margin: 10px;
  border-radius: 10px;
  border: none;
}
.f { background: #4CAF50; }
.b { background: #f44336; }
.l { background: #2196F3; }
.r { background: #FF9800; }
.s { background: #9E9E9E; width: 260px; }

.sensor-box {
  margin-top: 20px;
  padding: 10px;
  border-radius: 10px;
  background: #222;
  display: inline-block;
}
</style>
</head>
<body>

<h2>ESP32 Robotic Car</h2>
<p>Tap buttons to control</p>

<div class="sensor-box">
  <h3>Environment</h3>
  <p>Temperature: %TEMP% &deg;C</p>
  <p>Humidity: %HUM% %</p>
</div>

<br><br>

<button class="btn f" onclick="sendCmd('forward')">Forward</button><br>
<button class="btn l" onclick="sendCmd('left')">Left</button>
<button class="btn r" onclick="sendCmd('right')">Right</button><br>
<button class="btn b" onclick="sendCmd('backward')">Backward</button><br>
<button class="btn s" onclick="sendCmd('stop')">STOP</button>

<script>
function sendCmd(cmd){
  fetch('/' + cmd);
}
</script>

</body>
</html>
)rawliteral";

  // Replace placeholders with actual values
  page.replace("%TEMP%", String(t, 1));
  page.replace("%HUM%", String(h, 1));

  return page;
}

// ---------- REQUEST HANDLERS ----------
void handleRoot() {
  // Read updated DHT values before serving page
  readDHT();
  server.send(200, "text/html", htmlPage(temperature, humidity));
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Car Starting...");

  // Motor pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  stopCar();

  // DHT start
  dht.begin();

  // Bluetooth (HC-05)
  BT.begin(9600, SERIAL_8N1, BT_RX, BT_TX);
  Serial.println("Bluetooth (HC-05) ready at 9600 baud");

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("Open browser: http://");
  Serial.println(WiFi.localIP());

  // Routes
  server.on("/", handleRoot);
  server.on("/forward", []() {
    forward();
    server.send(200, "text/plain", "Forward");
  });
  server.on("/backward", []() {
    backward();
    server.send(200, "text/plain", "Backward");
  });
  server.on("/left", []() {
    left();
    server.send(200, "text/plain", "Left");
  });
  server.on("/right", []() {
    right();
    server.send(200, "text/plain", "Right");
  });
  server.on("/stop", []() {
    stopCar();
    server.send(200, "text/plain", "Stop");
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  handleBluetooth();  // continuously listen for voice commands
}
