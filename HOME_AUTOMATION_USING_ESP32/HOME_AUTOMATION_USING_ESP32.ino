
/*****************************************
 ESP32 LOCAL WEB SERVER + 4 RELAY
 WiFi IP Address Control
 No Adafruit IO Required
******************************************/

#include <WiFi.h>
#include <WebServer.h>

// WiFi credentials
#define WIFI_SSID "homeiot"
#define WIFI_PASS "homeiot123"

// Relay GPIO pins
#define RELAY1 25
#define RELAY2 26
#define RELAY3 27
#define RELAY4 14

// Active LOW relay board
#define RELAY_ON LOW
#define RELAY_OFF HIGH

WebServer server(80);

// Relay pins and states
const uint8_t relayPins[4] = {
  RELAY1, RELAY2, RELAY3, RELAY4
};

bool relayStates[4] = {
  false, false, false, false
};

// Set relay state
void setRelay(int index, bool state) {
  if (index < 0 || index >= 4) return;

  relayStates[index] = state;

  digitalWrite(
    relayPins[index],
    state ? RELAY_ON : RELAY_OFF
  );

  Serial.printf(
    "Relay %d: %s\n",
    index + 1,
    state ? "ON" : "OFF"
  );
}

// HTML dashboard
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport"
 content="width=device-width,initial-scale=1">

<title>ESP32 Smart Home</title>

<style>
* { box-sizing:border-box; }

body {
  margin:0;
  font-family:Arial,sans-serif;
  background:#101827;
  color:white;
  text-align:center;
}

header {
  background:#182337;
  padding:24px 12px;
}

h1 {
  margin:0;
  font-size:27px;
}

.subtitle {
  color:#a1aabd;
  margin-top:10px;
}

.container {
  display:grid;
  grid-template-columns:repeat(2,1fr);
  gap:16px;
  max-width:650px;
  margin:35px auto;
  padding:15px;
}

.card {
  background:#1f2937;
  padding:24px 12px;
  border-radius:14px;
  box-shadow:0 4px 12px #0003;
}

.card h2 {
  margin:0 0 15px;
}

.status {
  font-weight:bold;
  margin-bottom:18px;
}

.on { color:#4ade80; }
.off { color:#f87171; }

button {
  width:100%;
  max-width:200px;
  padding:14px;
  border:0;
  border-radius:8px;
  font-size:16px;
  font-weight:bold;
  cursor:pointer;
  color:white;
  background:#15803d;
}

button.active {
  background:#b91c1c;
}

button:disabled {
  opacity:0.6;
  cursor:wait;
}

footer {
  color:#9ca3af;
  margin:25px 0;
}

@media(max-width:450px) {
  .container { gap:10px; }
  .card { padding:20px 10px; }
}
</style>
</head>

<body>

<header>
<h1>ESP32 Smart Home</h1>
<div class="subtitle">
 Local WiFi Relay Controller
</div>
</header>

<div class="container">

<div class="card">
<h2>Relay 1</h2>
<div id="status1" class="status off">OFF</div>
<button id="btn1" onclick="toggleRelay(1)">
 TURN ON
</button>
</div>

<div class="card">
<h2>Relay 2</h2>
<div id="status2" class="status off">OFF</div>
<button id="btn2" onclick="toggleRelay(2)">
 TURN ON
</button>
</div>

<div class="card">
<h2>Relay 3</h2>
<div id="status3" class="status off">OFF</div>
<button id="btn3" onclick="toggleRelay(3)">
 TURN ON
</button>
</div>

<div class="card">
<h2>Relay 4</h2>
<div id="status4" class="status off">OFF</div>
<button id="btn4" onclick="toggleRelay(4)">
 TURN ON
</button>
</div>

</div>

<footer>
ESP32 Local Automation System
</footer>

<script>
let states = [false,false,false,false];

function updateUI() {
  for(let i=1;i<=4;i++) {
    let on = states[i-1];

    let status = document.getElementById(
      "status"+i
    );

    let btn = document.getElementById(
      "btn"+i
    );

    status.innerText = on ? "ON" : "OFF";
    status.className =
      "status " + (on ? "on" : "off");

    btn.innerText =
      on ? "TURN OFF" : "TURN ON";

    btn.className = on ? "active" : "";
  }
}

async function refreshStatus() {
  try {
    let response = await fetch(
      "/status",
      {cache:"no-store"}
    );

    if(!response.ok) throw new Error("HTTP error");

    states = await response.json();
    updateUI();
  } catch(error) {
    console.log("Status error:",error);
  }
}

async function toggleRelay(n) {
  let button = document.getElementById("btn"+n);
  button.disabled = true;

  try {
    let nextState = states[n-1] ? 0 : 1;

    let response = await fetch(
      "/relay?id="+n+"&state="+nextState,
      {method:"POST"}
    );

    if(!response.ok) throw new Error("Control failed");

    await refreshStatus();
  } catch(error) {
    alert("Cannot communicate with ESP32");
  } finally {
    button.disabled = false;
  }
}

// Refresh states every 2 seconds
setInterval(refreshStatus,2000);
refreshStatus();
</script>

</body>
</html>
)rawliteral";

// Main dashboard
void handleRoot() {
  server.send_P(
    200,
    "text/html",
    webpage
  );
}

// Relay API
void handleRelay() {
  if(!server.hasArg("id") ||
     !server.hasArg("state")) {
    server.send(400,"text/plain","Missing arguments");
    return;
  }

  String idText = server.arg("id");
  String stateText = server.arg("state");

  if(idText.length() != 1 ||
     idText[0] < '1' ||
     idText[0] > '4' ||
     (stateText != "0" && stateText != "1")) {
    server.send(400,"text/plain","Invalid arguments");
    return;
  }

  int id = idText.toInt();
  bool state = stateText == "1";

  setRelay(id-1,state);

  server.send(200,"text/plain","OK");
}

// Return all relay states
void handleStatus() {
  String json = "[";

  for(int i=0;i<4;i++) {
    json += relayStates[i] ? "true" : "false";

    if(i<3) json += ",";
  }

  json += "]";

  server.send(
    200,
    "application/json",
    json
  );
}

void setup() {
  Serial.begin(115200);

  // Initialize relays to OFF.
  // This cannot prevent all boot-time glitches
  // on every relay module.
  for(int i=0;i<4;i++) {
    digitalWrite(relayPins[i],RELAY_OFF);
    pinMode(relayPins[i],OUTPUT);
    relayStates[i] = false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID,WIFI_PASS);

  Serial.print("Connecting to WiFi");

  while(WiFi.status()!=WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");

  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // Register server routes
  server.on("/",HTTP_GET,handleRoot);
  server.on("/relay",HTTP_POST,handleRelay);
  server.on("/status",HTTP_GET,handleStatus);

  server.onNotFound([]() {
    server.send(404,"text/plain","Not Found");
  });

  server.begin();

  Serial.println("Local Web Server Started");
}

void loop() {
  server.handleClient();
}
