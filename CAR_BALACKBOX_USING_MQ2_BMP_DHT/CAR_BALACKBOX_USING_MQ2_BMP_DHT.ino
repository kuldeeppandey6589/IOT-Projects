/***************************************************
   ESP32 Robotic Car Local IP Web Server
   Ultrasonic Sensor + MQ2 Sensor + Buzzer
   DHT11 + BMP280 + Limit Switch Relay
   SD Card CSV Data Logger

   Webpage does NOT reload again and again.
   Only sensor values update using /data API.
   Data stores in SD card as sensor_log.csv.
****************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_BMP280.h>
#include <SPI.h>
#include <SD.h>

// =======================
// WiFi Details
// =======================
#define WIFI_SSID       "homeiot"
#define WIFI_PASS       "homeiot123"

// =======================
// Local Web Server
// =======================
WebServer server(80);

// =======================
// Motor Driver Pins
// =======================
#define IN1  27
#define IN2  26
#define IN3  25
#define IN4  33

#define ENA  14
#define ENB  32

// =======================
// Ultrasonic Sensor Pins
// =======================
#define TRIG_PIN  5
#define ECHO_PIN  18

// =======================
// MQ2 Sensor + Buzzer
// =======================
#define MQ2_PIN      34
#define BUZZER_PIN   4

// =======================
// DHT11 Sensor
// =======================
#define DHT_PIN   19
#define DHT_TYPE  DHT11

DHT dht(DHT_PIN, DHT_TYPE);

// =======================
// BMP280 Sensor
// SDA = GPIO 21
// SCL = GPIO 22
// =======================
Adafruit_BMP280 bmp;
bool bmpFound = false;

// =======================
// Limit Switch + Relay
// =======================
#define LIMIT_SWITCH_PIN  13
#define LIMIT_RELAY_PIN   23

bool limitRelayState = false;

// =======================
// SD Card Module Pins
// =======================
#define SD_SCK   16
#define SD_MISO  17
#define SD_MOSI  2
#define SD_CS    15

SPIClass sdSPI(HSPI);
bool sdReady = false;

const char* logFileName = "/sensor_log.csv";

// =======================
// Threshold Settings
// =======================
int mq2Threshold = 1800;
int objectDistanceLimit = 20;

// =======================
// Sensor Values
// =======================
int mq2Value = 0;
long distanceValue = 999;

float dhtTemperature = 0;
float dhtHumidity = 0;

float bmpTemperature = 0;
float bmpPressure = 0;
float bmpAltitude = 0;

String carStatus = "STOP";
String alertStatus = "Normal";
String dhtStatus = "Waiting";
String bmpStatus = "Waiting";
String limitSwitchStatus = "HIGH";
String relayStatus = "OFF";

// =======================
// Old Values for Change Detection
// =======================
int oldMq2Value = -1;
long oldDistanceValue = -1;

float oldDhtTemperature = -1000;
float oldDhtHumidity = -1000;
float oldBmpTemperature = -1000;
float oldBmpPressure = -1000;
float oldBmpAltitude = -1000;

String oldCarStatus = "";
String oldAlertStatus = "";
String oldDhtStatus = "";
String oldBmpStatus = "";
String oldLimitSwitchStatus = "";
String oldRelayStatus = "";

// =======================
// Motor Functions
// =======================

void forwardCar() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  carStatus = "FORWARD";
  Serial.println("Car Forward");
}

void backwardCar() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  carStatus = "BACKWARD";
  Serial.println("Car Backward");
}

void leftCar() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  carStatus = "LEFT";
  Serial.println("Car Left");
}

void rightCar() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  carStatus = "RIGHT";
  Serial.println("Car Right");
}

void stopCar() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  carStatus = "STOP";
  Serial.println("Car Stop");
}

// =======================
// Relay Function
// Active LOW relay module
// Relay ON  = LOW
// Relay OFF = HIGH
// =======================

void setLimitRelay(bool state) {
  limitRelayState = state;

  if (state == true) {
    digitalWrite(LIMIT_RELAY_PIN, LOW);
    relayStatus = "ON";
  } else {
    digitalWrite(LIMIT_RELAY_PIN, HIGH);
    relayStatus = "OFF";
  }
}

// =======================
// Limit Switch Function
// Switch HIGH = Relay OFF
// Switch LOW  = Relay ON
// =======================

void checkLimitSwitch() {
  int switchState = digitalRead(LIMIT_SWITCH_PIN);

  if (switchState == HIGH) {
    limitSwitchStatus = "HIGH";
    setLimitRelay(false);
  } else {
    limitSwitchStatus = "LOW";
    setLimitRelay(true);
  }
}

// =======================
// Ultrasonic Distance Function
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
// MQ2 + Ultrasonic Check
// =======================

void checkMainSensors() {
  mq2Value = analogRead(MQ2_PIN);
  distanceValue = getDistanceCM();

  bool gasDetected = mq2Value > mq2Threshold;
  bool objectDetected = distanceValue <= objectDistanceLimit;

  if (gasDetected || objectDetected) {
    digitalWrite(BUZZER_PIN, HIGH);

    if (gasDetected && objectDetected) {
      alertStatus = "Gas/Smoke + Object Detected";
    } else if (gasDetected) {
      alertStatus = "MQ2 Gas/Smoke Detected";
    } else if (objectDetected) {
      alertStatus = "Object Detected";
    }

    if (objectDetected) {
      stopCar();
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    alertStatus = "Normal";
  }

  Serial.print("MQ2 Value: ");
  Serial.print(mq2Value);
  Serial.print(" | Distance: ");
  Serial.print(distanceValue);
  Serial.print(" cm | Alert: ");
  Serial.println(alertStatus);
}

// =======================
// DHT11 + BMP280 Check
// =======================

void checkEnvironmentSensors() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    dhtStatus = "DHT11 Error";
  } else {
    dhtHumidity = h;
    dhtTemperature = t;
    dhtStatus = "OK";
  }

  if (bmpFound) {
    bmpTemperature = bmp.readTemperature();
    bmpPressure = bmp.readPressure() / 100.0F;
    bmpAltitude = bmp.readAltitude(1013.25);
    bmpStatus = "OK";
  } else {
    bmpStatus = "BMP280 Not Found";
  }

  Serial.print("DHT Temp: ");
  Serial.print(dhtTemperature);
  Serial.print(" C | Humidity: ");
  Serial.print(dhtHumidity);
  Serial.print(" % | BMP Temp: ");
  Serial.print(bmpTemperature);
  Serial.print(" C | Pressure: ");
  Serial.print(bmpPressure);
  Serial.print(" hPa | Altitude: ");
  Serial.print(bmpAltitude);
  Serial.println(" m");
}

// =======================
// SD Card Functions
// =======================

void setupSDCard() {
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("SD Card Mount Failed!");
    sdReady = false;
    return;
  }

  sdReady = true;
  Serial.println("SD Card Ready");

  if (!SD.exists(logFileName)) {
    File file = SD.open(logFileName, FILE_WRITE);

    if (file) {
      file.println("millis,car_status,mq2_value,distance_cm,alert_status,dht_status,dht_temperature,dht_humidity,bmp_status,bmp_temperature,bmp_pressure,bmp_altitude,limit_switch,relay_status");
      file.close();
      Serial.println("CSV Header Created");
    } else {
      Serial.println("CSV File Create Failed");
    }
  }
}

void logDataToSD() {
  if (!sdReady) {
    return;
  }

  File file = SD.open(logFileName, FILE_APPEND);

  if (!file) {
    Serial.println("Failed to open CSV file");
    return;
  }

  file.print(millis());
  file.print(",");
  file.print(carStatus);
  file.print(",");
  file.print(mq2Value);
  file.print(",");
  file.print(distanceValue);
  file.print(",");
  file.print(alertStatus);
  file.print(",");
  file.print(dhtStatus);
  file.print(",");
  file.print(dhtTemperature);
  file.print(",");
  file.print(dhtHumidity);
  file.print(",");
  file.print(bmpStatus);
  file.print(",");
  file.print(bmpTemperature);
  file.print(",");
  file.print(bmpPressure);
  file.print(",");
  file.print(bmpAltitude);
  file.print(",");
  file.print(limitSwitchStatus);
  file.print(",");
  file.println(relayStatus);

  file.close();

  Serial.println("Data saved to SD card");
}

bool sensorValueChanged() {
  bool changed = false;

  if (abs(mq2Value - oldMq2Value) >= 20) changed = true;
  if (abs(distanceValue - oldDistanceValue) >= 1) changed = true;

  if (abs(dhtTemperature - oldDhtTemperature) >= 0.5) changed = true;
  if (abs(dhtHumidity - oldDhtHumidity) >= 1.0) changed = true;

  if (abs(bmpTemperature - oldBmpTemperature) >= 0.5) changed = true;
  if (abs(bmpPressure - oldBmpPressure) >= 1.0) changed = true;
  if (abs(bmpAltitude - oldBmpAltitude) >= 1.0) changed = true;

  if (carStatus != oldCarStatus) changed = true;
  if (alertStatus != oldAlertStatus) changed = true;
  if (dhtStatus != oldDhtStatus) changed = true;
  if (bmpStatus != oldBmpStatus) changed = true;
  if (limitSwitchStatus != oldLimitSwitchStatus) changed = true;
  if (relayStatus != oldRelayStatus) changed = true;

  if (changed) {
    oldMq2Value = mq2Value;
    oldDistanceValue = distanceValue;

    oldDhtTemperature = dhtTemperature;
    oldDhtHumidity = dhtHumidity;

    oldBmpTemperature = bmpTemperature;
    oldBmpPressure = bmpPressure;
    oldBmpAltitude = bmpAltitude;

    oldCarStatus = carStatus;
    oldAlertStatus = alertStatus;
    oldDhtStatus = dhtStatus;
    oldBmpStatus = bmpStatus;
    oldLimitSwitchStatus = limitSwitchStatus;
    oldRelayStatus = relayStatus;
  }

  return changed;
}

// =======================
// JSON Data API
// =======================

String getJsonData() {
  String json = "{";

  json += "\"carStatus\":\"" + carStatus + "\",";
  json += "\"mq2Value\":" + String(mq2Value) + ",";
  json += "\"distanceValue\":" + String(distanceValue) + ",";
  json += "\"alertStatus\":\"" + alertStatus + "\",";

  json += "\"dhtStatus\":\"" + dhtStatus + "\",";
  json += "\"dhtTemperature\":" + String(dhtTemperature, 1) + ",";
  json += "\"dhtHumidity\":" + String(dhtHumidity, 1) + ",";

  json += "\"bmpStatus\":\"" + bmpStatus + "\",";
  json += "\"bmpTemperature\":" + String(bmpTemperature, 1) + ",";
  json += "\"bmpPressure\":" + String(bmpPressure, 1) + ",";
  json += "\"bmpAltitude\":" + String(bmpAltitude, 1) + ",";

  json += "\"limitSwitchStatus\":\"" + limitSwitchStatus + "\",";
  json += "\"relayStatus\":\"" + relayStatus + "\",";
  json += "\"sdStatus\":\"" + String(sdReady ? "SD OK" : "SD Error") + "\"";

  json += "}";

  return json;
}

// =======================
// Web Page HTML
// =======================

String webPage() {
  String page = "";

  page += "<!DOCTYPE html>";
  page += "<html>";
  page += "<head>";
  page += "<title>ESP32 Robotic Car</title>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";

  page += "<style>";
  page += "body{font-family:Arial;text-align:center;background:#111;color:white;margin:0;padding:20px;}";
  page += "h1{color:#00ff99;}";
  page += ".box{background:#222;padding:15px;margin:15px auto;border-radius:12px;max-width:430px;}";
  page += "button{width:130px;height:55px;margin:8px;font-size:18px;border:0;border-radius:10px;font-weight:bold;}";
  page += ".f{background:#00c853;color:white;}";
  page += ".b{background:#2962ff;color:white;}";
  page += ".l{background:#ffab00;color:black;}";
  page += ".r{background:#ff6d00;color:white;}";
  page += ".s{background:#d50000;color:white;width:280px;}";
  page += ".data{font-size:18px;line-height:30px;text-align:left;}";
  page += ".title{text-align:center;color:#00ff99;font-size:22px;font-weight:bold;}";
  page += ".value{color:#00e5ff;}";
  page += ".alert{color:#ff5252;font-weight:bold;}";
  page += "</style>";

  page += "</head>";
  page += "<body>";

  page += "<h1>ESP32 Robotic Car</h1>";

  page += "<div class='box'>";
  page += "<button class='f' onclick=\"sendCommand('/forward')\">Forward</button><br>";
  page += "<button class='l' onclick=\"sendCommand('/left')\">Left</button>";
  page += "<button class='r' onclick=\"sendCommand('/right')\">Right</button><br>";
  page += "<button class='b' onclick=\"sendCommand('/backward')\">Backward</button><br>";
  page += "<button class='s' onclick=\"sendCommand('/stop')\">STOP</button>";
  page += "</div>";

  page += "<div class='box data'>";
  page += "<div class='title'>Car Data</div>";
  page += "<b>Car Status:</b> <span class='value' id='carStatus'>--</span><br>";
  page += "<b>MQ2 Value:</b> <span class='value' id='mq2Value'>--</span><br>";
  page += "<b>Distance:</b> <span class='value' id='distanceValue'>--</span> cm<br>";
  page += "<b>Alert:</b> <span class='alert' id='alertStatus'>--</span><br>";
  page += "</div>";

  page += "<div class='box data'>";
  page += "<div class='title'>DHT11 Data</div>";
  page += "<b>DHT Status:</b> <span class='value' id='dhtStatus'>--</span><br>";
  page += "<b>Temperature:</b> <span class='value' id='dhtTemperature'>--</span> C<br>";
  page += "<b>Humidity:</b> <span class='value' id='dhtHumidity'>--</span> %<br>";
  page += "</div>";

  page += "<div class='box data'>";
  page += "<div class='title'>BMP280 Data</div>";
  page += "<b>BMP Status:</b> <span class='value' id='bmpStatus'>--</span><br>";
  page += "<b>Temperature:</b> <span class='value' id='bmpTemperature'>--</span> C<br>";
  page += "<b>Pressure:</b> <span class='value' id='bmpPressure'>--</span> hPa<br>";
  page += "<b>Altitude:</b> <span class='value' id='bmpAltitude'>--</span> m<br>";
  page += "</div>";

  page += "<div class='box data'>";
  page += "<div class='title'>Limit Switch Relay</div>";
  page += "<b>Limit Switch:</b> <span class='value' id='limitSwitchStatus'>--</span><br>";
  page += "<b>Relay Status:</b> <span class='value' id='relayStatus'>--</span><br>";
  page += "<b>SD Card:</b> <span class='value' id='sdStatus'>--</span><br>";
  page += "<small>Switch HIGH = Relay OFF<br>Switch LOW = Relay ON</small>";
  page += "</div>";

  page += "<script>";

  page += "let oldData = '';";

  page += "function updateText(id, value){";
  page += "let element = document.getElementById(id);";
  page += "if(element && element.innerHTML != value){";
  page += "element.innerHTML = value;";
  page += "}";
  page += "}";

  page += "function loadData(){";
  page += "fetch('/data')";
  page += ".then(response => response.json())";
  page += ".then(data => {";
  page += "let newData = JSON.stringify(data);";

  page += "if(newData !== oldData){";
  page += "oldData = newData;";

  page += "updateText('carStatus', data.carStatus);";
  page += "updateText('mq2Value', data.mq2Value);";
  page += "updateText('distanceValue', data.distanceValue);";
  page += "updateText('alertStatus', data.alertStatus);";

  page += "updateText('dhtStatus', data.dhtStatus);";
  page += "updateText('dhtTemperature', data.dhtTemperature);";
  page += "updateText('dhtHumidity', data.dhtHumidity);";

  page += "updateText('bmpStatus', data.bmpStatus);";
  page += "updateText('bmpTemperature', data.bmpTemperature);";
  page += "updateText('bmpPressure', data.bmpPressure);";
  page += "updateText('bmpAltitude', data.bmpAltitude);";

  page += "updateText('limitSwitchStatus', data.limitSwitchStatus);";
  page += "updateText('relayStatus', data.relayStatus);";
  page += "updateText('sdStatus', data.sdStatus);";
  page += "}";

  page += "})";
  page += ".catch(error => console.log('Data error:', error));";
  page += "}";

  page += "function sendCommand(command){";
  page += "fetch(command)";
  page += ".then(response => response.text())";
  page += ".then(result => {";
  page += "console.log(result);";
  page += "loadData();";
  page += "});";
  page += "}";

  page += "loadData();";
  page += "setInterval(loadData, 1000);";

  page += "</script>";

  page += "</body>";
  page += "</html>";

  return page;
}

// =======================
// Web Server Routes
// =======================

void handleRoot() {
  server.send(200, "text/html", webPage());
}

void handleData() {
  server.send(200, "application/json", getJsonData());
}

void handleForward() {
  forwardCar();
  server.send(200, "text/plain", "Forward");
}

void handleBackward() {
  backwardCar();
  server.send(200, "text/plain", "Backward");
}

void handleLeft() {
  leftCar();
  server.send(200, "text/plain", "Left");
}

void handleRight() {
  rightCar();
  server.send(200, "text/plain", "Right");
}

void handleStop() {
  stopCar();
  server.send(200, "text/plain", "Stop");
}

// =======================
// Setup
// =======================

void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(MQ2_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(LIMIT_SWITCH_PIN, INPUT_PULLUP);
  pinMode(LIMIT_RELAY_PIN, OUTPUT);
  digitalWrite(LIMIT_RELAY_PIN, HIGH);

  digitalWrite(ENA, HIGH);
  digitalWrite(ENB, HIGH);

  stopCar();

  dht.begin();

  Wire.begin(21, 22);

  bmpFound = bmp.begin(0x76);

  if (!bmpFound) {
    bmpFound = bmp.begin(0x77);
  }

  if (bmpFound) {
    Serial.println("BMP280 Found");
  } else {
    Serial.println("BMP280 Not Found");
  }

  setupSDCard();

  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");
  Serial.print("Open this IP in browser: http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);

  server.on("/forward", handleForward);
  server.on("/backward", handleBackward);
  server.on("/left", handleLeft);
  server.on("/right", handleRight);
  server.on("/stop", handleStop);

  server.begin();
  Serial.println("Local Web Server Started");
}

// =======================
// Main Loop
// =======================

void loop() {
  server.handleClient();

  checkLimitSwitch();

  static unsigned long lastMainSensorRead = 0;
  static unsigned long lastEnvironmentRead = 0;

  if (millis() - lastMainSensorRead >= 500) {
    lastMainSensorRead = millis();
    checkMainSensors();
  }

  if (millis() - lastEnvironmentRead >= 2000) {
    lastEnvironmentRead = millis();
    checkEnvironmentSensors();
  }

  if (sensorValueChanged()) {
    logDataToSD();
  }
}