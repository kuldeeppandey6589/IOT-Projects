#include <WiFi.h>
#include <WebServer.h>

// =======================
// WiFi Details
// =======================
const char* ssid = "homeiot";
const char* password = "homeiot123";

// =======================
// Pin Connections
// =======================
#define TRIG_PIN     4
#define ECHO_PIN     2
 
#define FLAME_PIN    34   // Use DO pin of flame sensor if digital, or AO if analog
#define RELAY_PIN    26
#define BUZZER_PIN   27

// =======================
// Settings
// =======================
#define OBJECT_DISTANCE_CM 30   // If object is closer than 20 cm, buzzer ON
#define FLAME_THRESHOLD    1000 // For analog flame sensor, adjust after testing

// Relay type
// Most relay modules are ACTIVE LOW
#define RELAY_ON   LOW
#define RELAY_OFF  HIGH

WebServer server(80);

bool objectDetected = false;
bool fireDetected = false;
bool relayState = false;
bool manualRelay = false;

// =======================
// Ultrasonic Distance
// =======================
long getDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    return 999;
  }

  long distance = duration * 0.034 / 2;
  return distance;
}

// =======================
// Sensor Reading
// =======================
void readSensors() {
  long distance = getDistanceCM();

  if (distance <= OBJECT_DISTANCE_CM) {
    objectDetected = true;
    Serial.print("distance : ");
    Serial.println(distance);
  } else {
    objectDetected = false;
  }

  int flameValue = analogRead(FLAME_PIN);

  // Many flame sensors give LOW value when flame is detected
  if (flameValue < FLAME_THRESHOLD) {
    fireDetected = true;
  } else {
    fireDetected = false;
  }

  // Buzzer logic
  if (objectDetected || fireDetected) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Relay logic
  // Fire detected = relay ON
  // Manual web relay can also turn relay ON
  if (fireDetected || manualRelay) {
    relayState = true;
    digitalWrite(RELAY_PIN, RELAY_ON);
  } else {
    relayState = false;
    digitalWrite(RELAY_PIN, RELAY_OFF);
  }
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
      max-width: 450px;
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
      Object Status: <span id="object">Loading...</span>
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
          document.getElementById("object").innerHTML =
            data.object ? "<span class='danger'>OBJECT DETECTED</span>" : "<span class='safe'>SAFE</span>";

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
  json += "\"object\":" + String(objectDetected ? "true" : "false") + ",";
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

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(FLAME_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, RELAY_OFF);
  digitalWrite(BUZZER_PIN, LOW);

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