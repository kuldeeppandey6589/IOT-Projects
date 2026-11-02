/*
  ESP32 BATTERY MONITORING + MOTOR CONTROL
  Board: ESP32 Dev Module

  Libraries:
  - DHT sensor library by Adafruit
  - Adafruit Unified Sensor

  CURRENT LOGIC (when USE_CALCULATED_CURRENT = true):
  Current is always shown (1 A to 5 A) in every condition.
  - Charging only: 9.0 V -> 5 A, 12.6 V -> 1 A
  - Discharging / Normal / Both ON: 9.0 V -> 1 A, 12.6 V -> 5 A
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <math.h>

// ==================== WIFI SETTINGS ====================

const char* WIFI_SSID = "homeiot";
const char* WIFI_PASS = "homeiot123";

// ==================== PIN CONFIGURATION ====================

#define VOLT_PIN             35
#define CURR_PIN             34
#define DHT_PIN               2
#define DHT_TYPE          DHT11

#define CHARGING_SWITCH      18
#define DISCHARGING_SWITCH   19
#define MOTOR_RELAY_PIN      23

// Change to false if your relay is active-HIGH.
const bool RELAY_ACTIVE_LOW = true;

// ==================== BATTERY SETTINGS ====================

const float PACK_EMPTY_V = 9.0;
const float PACK_FULL_V  = 12.6;

const float VOLT_DIVIDER_RATIO = 5.0;
const float VOLT_CAL = 1.00;

// ==================== CURRENT SETTINGS ====================

// true  = current calculated from battery voltage (1 A to 5 A)
// false = current measured by ACS712
const bool USE_CALCULATED_CURRENT = true;

const float CURRENT_MIN_A = 1.0;
const float CURRENT_MAX_A = 5.0;

const float ACS_SENS = 0.185;
const float ACS_DIV_RATIO = 20.0 / 30.0;
const float CURRENT_DEADBAND = 0.15;

// ==================== OBJECTS ====================

DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);

// ==================== GLOBAL VARIABLES ====================

float acsZeroV = 2.5;

float voltage = 0.0;
float current = 0.0;

float temperature = NAN;
float humidity = NAN;

int percent = 0;

String status = "Normal";

bool chargingON = false;
bool dischargingON = false;
bool motorON = false;

unsigned long lastSensorRead = 0;
unsigned long lastDhtRead = 0;
unsigned long lastWiFiRetry = 0;

const unsigned long SENSOR_INTERVAL = 500;
const unsigned long DHT_INTERVAL = 2200;

// ==================== ADC VOLTAGE READING ====================

float readPinVolts(int pin, int samples = 100) {
  uint32_t sum = 0;

  for (int i = 0; i < samples; i++) {
    sum += analogReadMilliVolts(pin);
    delayMicroseconds(200);
  }

  return (sum / (float)samples) / 1000.0;
}

// ==================== ACS712 CALIBRATION ====================

void calibrateACS712() {
  Serial.println();
  Serial.println("ACS712 calibration started.");
  Serial.println("Motor OFF. No current should flow.");

  delay(1500);

  acsZeroV = readPinVolts(CURR_PIN, 500) / ACS_DIV_RATIO;

  Serial.print("ACS712 zero-current voltage: ");
  Serial.print(acsZeroV, 3);
  Serial.println(" V");
}

// ==================== CURRENT FROM VOLTAGE ====================

float voltageFraction(float v) {
  float f = (v - PACK_EMPTY_V) / (PACK_FULL_V - PACK_EMPTY_V);
  return constrain(f, 0.0, 1.0);
}

// Discharging / Normal: 9.0 V -> 1 A, 12.6 V -> 5 A
float dischargeCurrentFromVoltage(float v) {
  return CURRENT_MIN_A +
         voltageFraction(v) * (CURRENT_MAX_A - CURRENT_MIN_A);
}

// Charging: 9.0 V -> 5 A, 12.6 V -> 1 A
float chargeCurrentFromVoltage(float v) {
  return CURRENT_MAX_A -
         voltageFraction(v) * (CURRENT_MAX_A - CURRENT_MIN_A);
}

// ==================== MOTOR RELAY CONTROL ====================

void setMotorRelay(bool turnOn) {
  motorON = turnOn;

  if (RELAY_ACTIVE_LOW) {
    digitalWrite(MOTOR_RELAY_PIN, turnOn ? LOW : HIGH);
  } else {
    digitalWrite(MOTOR_RELAY_PIN, turnOn ? HIGH : LOW);
  }
}

// ==================== SWITCH STATUS ====================

void updateBatteryStatus() {
  // Switch between GPIO and GND. LOW = ON, HIGH = OFF.

  chargingON = (digitalRead(CHARGING_SWITCH) == LOW);
  dischargingON = (digitalRead(DISCHARGING_SWITCH) == LOW);

  // Motor follows the discharging switch.
  setMotorRelay(dischargingON);

  if (chargingON && dischargingON) {
    status = "Warning";
  } else if (chargingON) {
    status = "Charging";
  } else if (dischargingON) {
    status = "Discharging";
  } else {
    status = "Normal";
  }
}

// ==================== READ SENSORS ====================

void readSensors() {
  // -------- BATTERY VOLTAGE --------

  voltage = readPinVolts(VOLT_PIN) * VOLT_DIVIDER_RATIO * VOLT_CAL;

  // -------- SWITCHES AND MOTOR --------

  updateBatteryStatus();

  // -------- CURRENT --------

  float rawCurrent = 0.0;

  if (USE_CALCULATED_CURRENT) {
    if (chargingON && !dischargingON) {
      // Charging only
      rawCurrent = chargeCurrentFromVoltage(voltage);
    } else {
      // Discharging, Normal, or Warning (both ON)
      rawCurrent = dischargeCurrentFromVoltage(voltage);
    }
  } else {
    float acsOutput = readPinVolts(CURR_PIN) / ACS_DIV_RATIO;
    rawCurrent = fabs((acsOutput - acsZeroV) / ACS_SENS);
  }

  static float filteredCurrent = 0.0;

  filteredCurrent = (0.70 * filteredCurrent) + (0.30 * rawCurrent);

  current = fabs(filteredCurrent);

  // Deadband only for real ACS712 measurement
  if (!USE_CALCULATED_CURRENT && current < CURRENT_DEADBAND) {
    current = 0.0;
  }

  if (USE_CALCULATED_CURRENT) {
    current = constrain(current, CURRENT_MIN_A, CURRENT_MAX_A);
  }

  // -------- BATTERY PERCENTAGE --------

  percent = constrain(
      (int)round((voltage - PACK_EMPTY_V) /
                 (PACK_FULL_V - PACK_EMPTY_V) * 100.0),
      0,
      100);

  // -------- DHT11 --------

  if (millis() - lastDhtRead >= DHT_INTERVAL) {
    lastDhtRead = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t)) temperature = t;
    if (!isnan(h)) humidity = h;
  }

  // -------- SERIAL MONITOR --------

  Serial.println("----------------------------");

  Serial.printf("Voltage: %.2f V\n", voltage);
  Serial.printf("Current: %.2f A\n", current);
  Serial.printf("Battery: %d %%\n", percent);

  if (!isnan(temperature)) {
    Serial.printf("Temperature: %.1f C\n", temperature);
  } else {
    Serial.println("Temperature: Sensor error");
  }

  if (!isnan(humidity)) {
    Serial.printf("Humidity: %.0f %%\n", humidity);
  } else {
    Serial.println("Humidity: Sensor error");
  }

  Serial.printf("Charging switch: %s\n", chargingON ? "ON" : "OFF");
  Serial.printf("Discharging switch: %s\n", dischargingON ? "ON" : "OFF");
  Serial.printf("Motor: %s\n", motorON ? "ON" : "OFF");
  Serial.printf("Status: %s\n", status.c_str());
}

// ==================== WEB DASHBOARD ====================

const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Battery Monitor</title>

<style>
* { box-sizing: border-box; }

body {
  font-family: Arial, sans-serif;
  background: #0b1220;
  color: white;
  margin: 0;
  padding: 20px;
}

.container {
  max-width: 850px;
  margin: auto;
}

h1 {
  text-align: center;
  font-size: 26px;
}

.subtitle {
  color: #94a3b8;
  text-align: center;
  margin-bottom: 22px;
}

.grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 14px;
}

.card {
  background: #172235;
  border-radius: 14px;
  padding: 20px;
  text-align: center;
  margin-bottom: 14px;
  overflow-wrap: anywhere;
}

.label {
  color: #94a3b8;
  font-size: 13px;
  letter-spacing: 1px;
}

.value {
  font-size: 28px;
  font-weight: bold;
  margin-top: 12px;
}

#status {
  font-size: 30px;
  font-weight: bold;
  margin-top: 12px;
}

.Charging, .on { color: #22c55e; }
.Discharging { color: #f87171; }
.Normal { color: #38bdf8; }
.Warning { color: #f59e0b; }
.off { color: #94a3b8; }

.bar {
  height: 18px;
  background: #334155;
  border-radius: 12px;
  overflow: hidden;
  margin-top: 15px;
}

#fill {
  height: 100%;
  width: 0%;
  background: #22c55e;
  transition: width 0.4s;
}

.footer {
  color: #94a3b8;
  text-align: center;
  font-size: 12px;
  margin-top: 20px;
}

@media (max-width: 480px) {
  body { padding: 12px; }
  h1 { font-size: 22px; }
  .card { padding: 14px 8px; }
  .value { font-size: 22px; }
  #status { font-size: 25px; }
}
</style>
</head>

<body>
<div class="container">

<h1>ESP32 Battery Monitoring</h1>

<div class="subtitle">
Battery Charging, Discharging and Motor Monitoring
</div>

<div class="card">
  <div class="label">BATTERY STATUS</div>
  <div id="status" class="Normal">Normal</div>
</div>

<div class="grid">
  <div class="card">
    <div class="label">CHARGING SWITCH</div>
    <div id="sw1" class="value off">OFF</div>
  </div>

  <div class="card">
    <div class="label">DISCHARGING SWITCH</div>
    <div id="sw2" class="value off">OFF</div>
  </div>

  <div class="card">
    <div class="label">BATTERY VOLTAGE</div>
    <div class="value"><span id="voltage">--</span> V</div>
  </div>

  <div class="card">
    <div class="label">BATTERY CURRENT</div>
    <div class="value"><span id="current">--</span> A</div>
  </div>

  <div class="card">
    <div class="label">TEMPERATURE</div>
    <div class="value"><span id="temp">--</span> &deg;C</div>
  </div>

  <div class="card">
    <div class="label">HUMIDITY</div>
    <div class="value"><span id="humidity">--</span> %</div>
  </div>

  <div class="card">
    <div class="label">MOTOR STATUS</div>
    <div id="motor" class="value off">OFF</div>
  </div>

  <div class="card">
    <div class="label">BATTERY LEVEL (APPROX.)</div>
    <div class="value"><span id="percent">--</span> %</div>
    <div class="bar"><div id="fill"></div></div>
  </div>
</div>

<div class="footer">
ESP32 Wi-Fi Battery Monitor | Refresh every 2 seconds
</div>

</div>

<script>
async function updateDashboard() {
  try {
    const response = await fetch('/data', { cache: 'no-store' });

    if (!response.ok) throw new Error('Connection failed');

    const d = await response.json();

    document.getElementById('voltage').textContent =
      Number(d.voltage).toFixed(2);

    document.getElementById('current').textContent =
      Math.abs(Number(d.current)).toFixed(2);

    document.getElementById('temp').textContent =
      d.temperature === null ? '--' : Number(d.temperature).toFixed(1);

    document.getElementById('humidity').textContent =
      d.humidity === null ? '--' : Number(d.humidity).toFixed(0);

    document.getElementById('percent').textContent = d.percent;
    document.getElementById('fill').style.width = d.percent + '%';

    const statusElement = document.getElementById('status');
    statusElement.textContent =
      d.status === 'Warning' ? 'Both Switches ON!' : d.status;
    statusElement.className = d.status;

    const sw1 = document.getElementById('sw1');
    sw1.textContent = d.chargingSwitch ? 'ON' : 'OFF';
    sw1.className = 'value ' + (d.chargingSwitch ? 'on' : 'off');

    const sw2 = document.getElementById('sw2');
    sw2.textContent = d.dischargingSwitch ? 'ON' : 'OFF';
    sw2.className = 'value ' + (d.dischargingSwitch ? 'on' : 'off');

    const motor = document.getElementById('motor');
    motor.textContent = d.motorOn ? 'ON' : 'OFF';
    motor.className = 'value ' + (d.motorOn ? 'on' : 'off');

  } catch (error) {
    const statusElement = document.getElementById('status');
    statusElement.textContent = 'Offline';
    statusElement.className = 'Normal';
  }
}

updateDashboard();
setInterval(updateDashboard, 2000);
</script>

</body>
</html>
)rawliteral";

// ==================== WEB SERVER ROUTES ====================

void handleRoot() {
  server.send_P(200, "text/html", PAGE);
}

void handleData() {
  updateBatteryStatus();

  String json = "{";

  json += "\"voltage\":";
  json += String(voltage, 2);
  json += ",";

  json += "\"current\":";
  json += String(fabs(current), 2);
  json += ",";

  json += "\"temperature\":";
  json += isnan(temperature) ? "null" : String(temperature, 1);
  json += ",";

  json += "\"humidity\":";
  json += isnan(humidity) ? "null" : String(humidity, 1);
  json += ",";

  json += "\"percent\":";
  json += String(percent);
  json += ",";

  json += "\"chargingSwitch\":";
  json += chargingON ? "true" : "false";
  json += ",";

  json += "\"dischargingSwitch\":";
  json += dischargingON ? "true" : "false";
  json += ",";

  json += "\"motorOn\":";
  json += motorON ? "true" : "false";
  json += ",";

  json += "\"status\":\"";
  json += status;
  json += "\"}";

  server.send(200, "application/json", json);
}

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);

  // Relay OFF at startup
  pinMode(MOTOR_RELAY_PIN, OUTPUT);
  setMotorRelay(false);

  // ADC configuration
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Switches
  pinMode(CHARGING_SWITCH, INPUT_PULLUP);
  pinMode(DISCHARGING_SWITCH, INPUT_PULLUP);

  // DHT sensor
  dht.begin();

  // ACS712 calibration only needed in measured mode
  if (!USE_CALCULATED_CURRENT) {
    calibrateACS712();
  }

  lastDhtRead = millis() - DHT_INTERVAL;
  readSensors();

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi connected.");

  Serial.print("Dashboard IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);

  server.begin();
  Serial.println("Web server started.");
}

// ==================== MAIN LOOP ====================

void loop() {
  server.handleClient();

  updateBatteryStatus();

  if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = millis();
    readSensors();
  }

  if (WiFi.status() != WL_CONNECTED &&
      millis() - lastWiFiRetry >= 10000) {
    lastWiFiRetry = millis();
    WiFi.reconnect();
  }
}