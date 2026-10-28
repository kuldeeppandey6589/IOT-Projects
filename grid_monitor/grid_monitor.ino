
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ---------- WiFi ----------
#define WLAN_SSID "homeiot"
#define WLAN_PASS "homeiot123"

// ---------- Pins ----------
#define POT_PIN     A0
#define RELAY1_PIN  D1
#define RELAY2_PIN  D2
#define BUZZER_PIN  D5

// Relay module configuration
#define RELAY_ACTIVE_LOW true

// Set true only for an active-LOW buzzer module
#define BUZZER_ACTIVE_LOW false

// ---------- Simulation scale ----------
#define MAX_VALUE    256
#define MAX_CURRENT  4.0
#define MAX_POWER    10.0

// ---------- Thresholds ----------
#define TH_NORMAL    75
#define TH_OVERLOAD  220
#define HYSTERESIS   8

// ---------- Buzzer timing ----------
#define BEEP_ON_TIME   300
#define BEEP_OFF_TIME  200

ESP8266WebServer server(80);

// ---------- Grid variables ----------
bool relay1State = false;
bool relay2State = false;

int potValue = 0;
float current = 0.0;
float power = 0.0;

String status = "No Load";

unsigned long lastRead = 0;
const unsigned long READ_INTERVAL = 200;

// ---------- Buzzer variables ----------
bool buzzerOn = false;
unsigned long lastBeep = 0;

// ---------- Web dashboard ----------
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Grid Monitor</title>

<style>
body {
  font-family: Arial, sans-serif;
  background: #0f172a;
  color: #e2e8f0;
  margin: 0;
  padding: 20px;
  text-align: center;
}
h1 {
  margin-bottom: 20px;
  font-size: 1.6em;
}
.card {
  background: #1e293b;
  border-radius: 14px;
  padding: 20px;
  max-width: 420px;
  margin: 15px auto;
}
.status {
  font-size: 2em;
  font-weight: bold;
  padding: 18px;
  border-radius: 12px;
}
.s-none { background: #475569; }
.s-normal { background: #16a34a; }
.s-over { background: #dc2626; animation: blink .8s infinite; }
@keyframes blink {
  50% { opacity: .5; }
}
.bar {
  background: #334155;
  border-radius: 10px;
  height: 26px;
  overflow: hidden;
  margin-top: 12px;
}
.fill {
  height: 100%;
  width: 0;
  background: #38bdf8;
  transition: width .3s;
}
.val {
  font-size: 2.4em;
  font-weight: bold;
}
.row {
  display: flex;
  gap: 12px;
  justify-content: center;
}
.relay {
  flex: 1;
  padding: 14px;
  border-radius: 12px;
  background: #475569;
  font-weight: bold;
}
.on { background: #f59e0b; color: #000; }
.box {
  flex: 1;
  background: #334155;
  border-radius: 12px;
  padding: 14px 5px;
}
.box .n {
  font-size: 1.8em;
  font-weight: bold;
}
small { color: #94a3b8; }
</style>
</head>

<body>
<h1>⚡ Grid Monitoring System</h1>

<div class="card">
  <div id="status" class="status s-none">No Load</div>
</div>

<div class="card">
  <small>Grid Level (simulated, 0-256)</small>
  <div class="val" id="val">0</div>
  <div class="bar">
    <div class="fill" id="fill"></div>
  </div>
  <small id="pct">0%</small>
</div>

<div class="card">
  <div class="row">
    <div class="box">
      <small>Current</small>
      <div class="n"><span id="cur">0.00</span> A</div>
    </div>
    <div class="box">
      <small>Power</small>
      <div class="n"><span id="pow">0.0</span> W</div>
    </div>
  </div>
</div>

<div class="card">
  <div class="row">
    <div class="relay" id="r1">Relay 1<br>OFF</div>
    <div class="relay" id="r2">Relay 2<br>OFF</div>
  </div>
</div>

<small id="conn">Connecting...</small>

<script>
async function update() {
  try {
    const response = await fetch('/data');
    if (!response.ok) throw new Error('HTTP error');

    const d = await response.json();

    document.getElementById('val').innerText = d.value;

    const p = Math.round(d.value / 256 * 100);
    document.getElementById('fill').style.width = p + '%';
    document.getElementById('pct').innerText = p + '%';

    document.getElementById('cur').innerText =
      Number(d.current).toFixed(2);

    document.getElementById('pow').innerText =
      Number(d.power).toFixed(1);

    const s = document.getElementById('status');
    s.innerText = d.status;

    if (d.status === 'OVERLOAD') {
      s.className = 'status s-over';
    } else if (d.status === 'Normal') {
      s.className = 'status s-normal';
    } else {
      s.className = 'status s-none';
    }

    const r1 = document.getElementById('r1');
    const r2 = document.getElementById('r2');

    r1.innerHTML = 'Relay 1<br>' + (d.r1 ? 'ON' : 'OFF');
    r1.className = 'relay' + (d.r1 ? ' on' : '');

    r2.innerHTML = 'Relay 2<br>' + (d.r2 ? 'ON' : 'OFF');
    r2.className = 'relay' + (d.r2 ? ' on' : '');

    document.getElementById('conn').innerText =
      'Live - updated ' + new Date().toLocaleTimeString();

  } catch (e) {
    document.getElementById('conn').innerText =
      'Connection lost - reconnecting...';
  }
}

update();
setInterval(update, 1000);
</script>
</body>
</html>
)rawliteral";

// ---------- Relay control ----------
void setRelay(uint8_t pin, bool on) {
  digitalWrite(
    pin,
    RELAY_ACTIVE_LOW ? (on ? LOW : HIGH)
                     : (on ? HIGH : LOW)
  );
}

// ---------- Buzzer control ----------
void setBuzzer(bool on) {
  buzzerOn = on;

  digitalWrite(
    BUZZER_PIN,
    BUZZER_ACTIVE_LOW ? (on ? LOW : HIGH)
                      : (on ? HIGH : LOW)
  );
}

void updateBuzzer() {
  if (status == "OVERLOAD") {
    unsigned long interval =
      buzzerOn ? BEEP_ON_TIME : BEEP_OFF_TIME;

    if (millis() - lastBeep >= interval) {
      lastBeep = millis();
      setBuzzer(!buzzerOn);
    }
  } else {
    if (buzzerOn) {
      setBuzzer(false);
    }
  }
}

// ---------- Web handlers ----------
void handleRoot() {
  server.send_P(200, "text/html", PAGE);
}

void handleData() {
  String json = "{\"value\":" + String(potValue) +
                ",\"current\":" + String(current, 2) +
                ",\"power\":" + String(power, 1) +
                ",\"r1\":" + String(relay1State ? "true" : "false") +
                ",\"r2\":" + String(relay2State ? "true" : "false") +
                ",\"status\":\"" + status + "\"}";

  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}

// ---------- Read sensor and control relays ----------
void readAndControl() {
  // Average 10 ADC readings
  long sum = 0;

  for (int i = 0; i < 10; i++) {
    sum += analogRead(POT_PIN);
    delay(1);
  }

  int adc = sum / 10;

  // Convert ADC reading to 0-256 simulation scale
  potValue = map(adc, 0, 1023, 0, MAX_VALUE);

  // Simulated current: 0-4 A
  current = (potValue / (float)MAX_VALUE) * MAX_CURRENT;

  // Simulated power: 0-10 W
  power = (potValue / (float)MAX_VALUE) * MAX_POWER;

  // Relay 1 hysteresis
  if (!relay1State && potValue > TH_NORMAL) {
    relay1State = true;
  } else if (relay1State &&
             potValue < TH_NORMAL - HYSTERESIS) {
    relay1State = false;
  }

  // Relay 2 hysteresis
  if (!relay2State && potValue > TH_OVERLOAD) {
    relay2State = true;
  } else if (relay2State &&
             potValue < TH_OVERLOAD - HYSTERESIS) {
    relay2State = false;
  }

  // Apply relay outputs
  setRelay(RELAY1_PIN, relay1State);
  setRelay(RELAY2_PIN, relay2State);

  // Determine status
  if (relay1State && relay2State) {
    status = "OVERLOAD";
  } else if (relay1State) {
    status = "Normal";
  } else {
    status = "No Load";
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initial safe output state
  setRelay(RELAY1_PIN, false);
  setRelay(RELAY2_PIN, false);
  setBuzzer(false);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WLAN_SSID, WLAN_PASS);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Dashboard URL: http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();

  Serial.println("Grid monitoring started");
}

// ---------- Main loop ----------
void loop() {
  server.handleClient();

  if (millis() - lastRead >= READ_INTERVAL) {
    lastRead = millis();

    readAndControl();

    Serial.printf(
      "Level: %d | Current: %.2f A | Power: %.1f W | "
      "R1: %s | R2: %s | Status: %s\n",
      potValue,
      current,
      power,
      relay1State ? "ON" : "OFF",
      relay2State ? "ON" : "OFF",
      status.c_str()
    );
  }

  updateBuzzer();
}
