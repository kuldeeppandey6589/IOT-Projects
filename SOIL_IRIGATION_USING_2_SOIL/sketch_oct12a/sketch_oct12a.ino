
#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

// ========== WIFI ==========
const char* ssid = "homeiot";
const char* password = "homeiot123";

// ========== SOIL SENSORS ==========
#define SOIL1_PIN 34
#define SOIL2_PIN 35

// ========== DHT11 ==========
#define DHT_PIN 4
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ========== RELAYS ==========
#define RELAY1_PIN 14
#define RELAY2_PIN 25
#define RELAY3_PIN 26
#define RELAY4_PIN 27

// Most relay modules are ACTIVE LOW
#define RELAY_ON LOW
#define RELAY_OFF HIGH

// ========== SOIL CALIBRATION ==========
const int SOIL_DRY_VALUE = 3000;
const int SOIL_WET_VALUE = 1300;
const int DRY_THRESHOLD = 35;

// ========== SERVER ==========
WebServer server(80);

// ========== SENSOR DATA ==========
int soilRaw1 = 0;
int soilRaw2 = 0;
int moisture1 = 0;
int moisture2 = 0;

float temperature = NAN;
float humidity = NAN;

bool pump1 = false;
bool pump2 = false;
bool pump3 = false;
bool pump4 = false;

// ========== READ SOIL MOISTURE ==========
int readMoisture(int pin, int &rawValue) {
  rawValue = analogRead(pin);

  int percent = map(
    rawValue,
    SOIL_DRY_VALUE,
    SOIL_WET_VALUE,
    0,
    100
  );

  return constrain(percent, 0, 100);
}

// ========== RELAY CONTROL ==========
void setRelay(int pin, bool &pumpState, bool shouldRun) {
  pumpState = shouldRun;
  digitalWrite(pin, shouldRun ? RELAY_ON : RELAY_OFF);
}

// ========== UPDATE SENSORS AND PUMPS ==========
void updateSystem() {
  moisture1 = readMoisture(SOIL1_PIN, soilRaw1);
  moisture2 = readMoisture(SOIL2_PIN, soilRaw2);

  // Soil sensor 1 controls two pumps
  bool dry1 = moisture1 < DRY_THRESHOLD;

  setRelay(RELAY1_PIN, pump1, dry1);
  setRelay(RELAY2_PIN, pump2, dry1);

  // Soil sensor 2 controls two pumps
  bool dry2 = moisture2 < DRY_THRESHOLD;

  setRelay(RELAY3_PIN, pump3, dry2);
  setRelay(RELAY4_PIN, pump4, dry2);

  Serial.println("\n===== IRRIGATION STATUS =====");

  Serial.printf("Soil 1: ADC=%d, Moisture=%d%%\n",
                soilRaw1, moisture1);

  Serial.printf("Soil 2: ADC=%d, Moisture=%d%%\n",
                soilRaw2, moisture2);

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT11: Reading unavailable");
  } else {
    Serial.printf("Temperature: %.1f C\n", temperature);
    Serial.printf("Humidity: %.1f %%\n", humidity);
  }

  Serial.printf("Pump 1=%s, Pump 2=%s\n",
                pump1 ? "ON" : "OFF",
                pump2 ? "ON" : "OFF");

  Serial.printf("Pump 3=%s, Pump 4=%s\n",
                pump3 ? "ON" : "OFF",
                pump4 ? "ON" : "OFF");
}

// ========== WEBPAGE ==========
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Smart Irrigation</title>

<style>
body {
  font-family: Arial, sans-serif;
  background: #eef5ee;
  color: #203528;
  margin: 0;
  text-align: center;
}
header {
  background: #176b3a;
  color: white;
  padding: 20px 10px;
}
.container {
  max-width: 900px;
  margin: auto;
  padding: 12px;
}
.grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(210px, 1fr));
  gap: 14px;
}
.card {
  background: white;
  border-radius: 12px;
  padding: 20px;
  box-shadow: 0 2px 8px #0001;
}
.value {
  font-size: 30px;
  font-weight: bold;
  color: #176b3a;
}
.status {
  font-weight: bold;
  font-size: 18px;
}
.on { color: #16843c; }
.off { color: #c0392b; }
small { color: #66776b; }
</style>
</head>

<body>
<header>
  <h1>Smart Irrigation System</h1>
  <p>ESP32 Live Monitoring Dashboard</p>
</header>

<div class="container">

<h2>Environment Monitoring</h2>
<div class="grid">
  <div class="card">
    <h3>Temperature</h3>
    <div class="value" id="temp">-- °C</div>
  </div>
  <div class="card">
    <h3>Humidity</h3>
    <div class="value" id="hum">-- %</div>
  </div>
</div>

<h2>Soil Moisture Monitoring</h2>
<div class="grid">
  <div class="card">
    <h3>Soil Sensor 1</h3>
    <div class="value" id="m1">--%</div>
    <p id="s1">Loading...</p>
    <small>ADC: <span id="r1">--</span></small>
    <p>Controls Pump 1 and Pump 2</p>
  </div>

  <div class="card">
    <h3>Soil Sensor 2</h3>
    <div class="value" id="m2">--%</div>
    <p id="s2">Loading...</p>
    <small>ADC: <span id="r2">--</span></small>
    <p>Controls Pump 3 and Pump 4</p>
  </div>
</div>

<h2>Pump Status</h2>
<div class="grid">
  <div class="card"><h3>Pump 1</h3><p id="p1" class="status">--</p></div>
  <div class="card"><h3>Pump 2</h3><p id="p2" class="status">--</p></div>
  <div class="card"><h3>Pump 3</h3><p id="p3" class="status">--</p></div>
  <div class="card"><h3>Pump 4</h3><p id="p4" class="status">--</p></div>
</div>

<p id="connection">Connecting to ESP32...</p>
<p><small>Dashboard refreshes every 2 seconds</small></p>
</div>

<script>
function setPump(id, state) {
  const el = document.getElementById(id);
  el.textContent = state ? "ON" : "OFF";
  el.className = "status " + (state ? "on" : "off");
}

async function refreshData() {
  try {
    const response = await fetch('/data');
    if (!response.ok) throw new Error("Connection error");

    const d = await response.json();

    document.getElementById('temp').textContent =
      d.temperature === null ? '-- °C' : d.temperature + ' °C';

    document.getElementById('hum').textContent =
      d.humidity === null ? '-- %' : d.humidity + ' %';

    document.getElementById('m1').textContent = d.moisture1 + '%';
    document.getElementById('m2').textContent = d.moisture2 + '%';

    document.getElementById('r1').textContent = d.raw1;
    document.getElementById('r2').textContent = d.raw2;

    document.getElementById('s1').textContent =
      d.moisture1 < d.threshold ? 'SOIL DRY' : 'MOISTURE OK';

    document.getElementById('s2').textContent =
      d.moisture2 < d.threshold ? 'SOIL DRY' : 'MOISTURE OK';

    setPump('p1', d.pump1);
    setPump('p2', d.pump2);
    setPump('p3', d.pump3);
    setPump('p4', d.pump4);

    document.getElementById('connection').textContent =
      'ESP32 connected - Live data received';
  } catch (e) {
    document.getElementById('connection').textContent =
      'Connection lost - Reconnecting...';
  }
}

refreshData();
setInterval(refreshData, 2000);
</script>
</body>
</html>
)rawliteral";

// ========== WEB SERVER ROUTES ==========
void handleRoot() {
  server.send_P(200, "text/html", webpage);
}

void handleData() {
  String json = "{";

  json += "\"raw1\":" + String(soilRaw1) + ",";
  json += "\"raw2\":" + String(soilRaw2) + ",";
  json += "\"moisture1\":" + String(moisture1) + ",";
  json += "\"moisture2\":" + String(moisture2) + ",";
  json += "\"threshold\":" + String(DRY_THRESHOLD) + ",";

  json += "\"temperature\":";
  json += isnan(temperature) ? "null" : String(temperature, 1);
  json += ",";

  json += "\"humidity\":";
  json += isnan(humidity) ? "null" : String(humidity, 1);
  json += ",";

  json += "\"pump1\":" + String(pump1 ? "true" : "false") + ",";
  json += "\"pump2\":" + String(pump2 ? "true" : "false") + ",";
  json += "\"pump3\":" + String(pump3 ? "true" : "false") + ",";
  json += "\"pump4\":" + String(pump4 ? "true" : "false");

  json += "}";

  server.send(200, "application/json", json);
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);

  // Pumps OFF during startup
  digitalWrite(RELAY1_PIN, RELAY_OFF);
  digitalWrite(RELAY2_PIN, RELAY_OFF);
  digitalWrite(RELAY3_PIN, RELAY_OFF);
  digitalWrite(RELAY4_PIN, RELAY_OFF);

  analogReadResolution(12);

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to Wi-Fi");

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected!");
    Serial.print("Open browser: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWi-Fi failed. Check credentials and reset.");
  }

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();

  // Initial readings
  moisture1 = readMoisture(SOIL1_PIN, soilRaw1);
  moisture2 = readMoisture(SOIL2_PIN, soilRaw2);

  // DHT11 requires time between readings
  delay(2000);
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  updateSystem();
}

// ========== MAIN LOOP ==========
void loop() {
  server.handleClient();

  static unsigned long lastSoilRead = 0;
  static unsigned long lastDhtRead = 0;

  // Update soil sensors and pump control every second
  if (millis() - lastSoilRead >= 1000) {
    lastSoilRead = millis();
    updateSystem();
  }

  // Read DHT11 every 2.5 seconds
  if (millis() - lastDhtRead >= 2500) {
    lastDhtRead = millis();

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // Retain last valid values if reading fails
    if (!isnan(h) && !isnan(t)) {
      humidity = h;
      temperature = t;
    } else {
      Serial.println("DHT11 reading failed. Check wiring.");
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }
}
