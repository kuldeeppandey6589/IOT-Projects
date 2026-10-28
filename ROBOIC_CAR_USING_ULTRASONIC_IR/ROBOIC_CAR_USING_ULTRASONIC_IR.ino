#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =====================================================
// WIFI
// =====================================================

const char* ssid = "Robotutor";
const char* password = "Robotutor";

WebServer server(80);

// =====================================================
// LCD 16x2 I2C
// =====================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================================
// IR SENSOR PINS
// =====================================================

#define IR1_PIN 34
#define IR2_PIN 35

// =====================================================
// ULTRASONIC SENSOR
// =====================================================

#define TRIG_PIN 5
#define ECHO_PIN 18

// =====================================================
// L298N MOTOR DRIVER
// =====================================================

// LEFT MOTOR
#define ENA 25
#define IN1 26
#define IN2 27

// RIGHT MOTOR
#define ENB 14
#define IN3 32
#define IN4 33

// =====================================================
// SETTINGS
// =====================================================

const float STOP_DISTANCE = 20.0;

// =====================================================
// VARIABLES
// =====================================================

int ir1State = LOW;
int ir2State = LOW;

float distanceCM = 999.0;

String motorState = "STOPPED";
String systemStatus = "STARTING";

// =====================================================
// LCD PRINT FUNCTION
// =====================================================

void printLCD(int row, String text) {

  if (text.length() > 16) {
    text = text.substring(0, 16);
  }

  while (text.length() < 16) {
    text += " ";
  }

  lcd.setCursor(0, row);
  lcd.print(text);
}

// =====================================================
// MOTOR STOP
// =====================================================

void stopMotor() {

  digitalWrite(ENA, LOW);
  digitalWrite(ENB, LOW);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  motorState = "STOPPED";
}

// =====================================================
// MOTOR FORWARD
// =====================================================

void moveForward() {

  // LEFT MOTOR FORWARD
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  // RIGHT MOTOR FORWARD
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  digitalWrite(ENA, HIGH);
  digitalWrite(ENB, HIGH);

  motorState = "RUNNING FORWARD";
}

// =====================================================
// ULTRASONIC DISTANCE
// =====================================================

float readDistance() {

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration =
      pulseIn(ECHO_PIN, HIGH, 30000);

  // No echo
  if (duration == 0) {
    return 999.0;
  }

  float distance =
      (duration * 0.0343) / 2.0;

  return distance;
}

// =====================================================
// VEHICLE CONTROL
// =====================================================

void controlVehicle() {

  // Read IR sensors
  ir1State = digitalRead(IR1_PIN);
  ir2State = digitalRead(IR2_PIN);

  // Read ultrasonic
  distanceCM = readDistance();

  // ===================================================
  // PRIORITY 1
  // OBJECT DETECTED
  // ===================================================

  if (distanceCM <= STOP_DISTANCE) {

    stopMotor();

    systemStatus = "OBJECT DETECTED";

    printLCD(0, "OBJECT DETECTED");

    printLCD(
      1,
      "Dist:" + String(distanceCM, 1) + "cm"
    );

    return;
  }

  // ===================================================
  // PRIORITY 2
  //
  // BOTH IR HIGH = MOTOR RUN
  // ===================================================

  if (ir1State == LOW &&
      ir2State == LOW) {

    moveForward();

    systemStatus = "TRACK OK - RUN";

    printLCD(0, "TRACK OK");

    printLCD(
      1,
      "MOTOR RUNNING"
    );

    return;
  }

  // ===================================================
  // IR1 HIGH + IR2 LOW
  // ===================================================

  if (ir1State == LOW &&
      ir2State == HIGH) {

    stopMotor();

    systemStatus = "IR2 TRACK FAULT";

    printLCD(0, "TRACK FAULT");

    printLCD(
      1,
      "IR2 NOT DETECT"
    );

    return;
  }

  // ===================================================
  // IR1 LOW + IR2 HIGH
  // ===================================================

  if (ir1State == HIGH &&
      ir2State == LOW) {

    stopMotor();

    systemStatus = "IR1 TRACK FAULT";

    printLCD(0, "TRACK FAULT");

    printLCD(
      1,
      "IR1 NOT DETECT"
    );

    return;
  }

  // ===================================================
  // BOTH IR LOW
  // ===================================================

  if (ir1State == HIGH &&
      ir2State == HIGH) {

    stopMotor();

    systemStatus = "TRACK NOT DETECTED";

    printLCD(0, "TRACK FAULT");

    printLCD(
      1,
      "BOTH IR LOW"
    );

    return;
  }
}

// =====================================================
// WEB PAGE
// =====================================================

String getWebPage() {

  String html = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
content="width=device-width, initial-scale=1">

<title>ESP32 Track Vehicle</title>

<style>

body
{
    margin:0;
    font-family:Arial,sans-serif;
    background:#0f172a;
    color:white;
}

header
{
    padding:25px;
    text-align:center;
    background:#1e293b;
}

header h1
{
    color:#38bdf8;
    margin:0;
}

.container
{
    max-width:900px;
    margin:auto;
    padding:20px;
}

.grid
{
    display:grid;
    grid-template-columns:
    repeat(auto-fit,minmax(200px,1fr));

    gap:15px;
}

.card
{
    background:#1e293b;
    padding:20px;
    margin-top:15px;
    border-radius:12px;
}

.value
{
    font-size:28px;
    font-weight:bold;
    color:#38bdf8;
}

.status
{
    font-size:28px;
    font-weight:bold;
}

.green
{
    color:#22c55e;
}

.red
{
    color:#ef4444;
}

.yellow
{
    color:#facc15;
}

</style>

</head>

<body>

<header>

<h1>ESP32 Track Vehicle</h1>

<p>Live Monitoring System</p>

</header>


<div class="container">


<div class="card">

<h2>System Status</h2>

<div id="status"
class="status">

Loading...

</div>

</div>


<div class="grid">


<div class="card">

<h3>IR Sensor 1</h3>

<div id="ir1"
class="value">

--

</div>

</div>


<div class="card">

<h3>IR Sensor 2</h3>

<div id="ir2"
class="value">

--

</div>

</div>


<div class="card">

<h3>Distance</h3>

<div id="distance"
class="value">

--

</div>

</div>


<div class="card">

<h3>Motor</h3>

<div id="motor"
class="value">

--

</div>

</div>


</div>


<div class="card">

<h3>ESP32 IP Address</h3>

<div id="ip"
class="value">

--

</div>

</div>


<div class="card">

<h3>Motor Logic</h3>

<p>

IR1 HIGH + IR2 HIGH =
<b style="color:#22c55e">
MOTOR RUN
</b>

</p>

<p>

IR1 HIGH + IR2 LOW =
<b style="color:#ef4444">
STOP
</b>

</p>

<p>

IR1 LOW + IR2 HIGH =
<b style="color:#ef4444">
STOP
</b>

</p>

<p>

IR1 LOW + IR2 LOW =
<b style="color:#ef4444">
STOP
</b>

</p>

<p>

Object within 20 cm =
<b style="color:#ef4444">
STOP
</b>

</p>

</div>


</div>


<script>

function updateData()
{

fetch('/data')

.then(response => response.json())

.then(data => {

document.getElementById("ir1").innerHTML =
data.ir1 == 1 ? "HIGH" : "LOW";

document.getElementById("ir2").innerHTML =
data.ir2 == 1 ? "HIGH" : "LOW";

document.getElementById("distance").innerHTML =
data.distance + " cm";

document.getElementById("motor").innerHTML =
data.motor;

document.getElementById("status").innerHTML =
data.status;

document.getElementById("ip").innerHTML =
data.ip;


// STATUS COLOR

let status =
document.getElementById("status");


if(data.motor == "RUNNING FORWARD")
{
    status.className =
    "status green";
}
else
{
    status.className =
    "status red";
}

})

.catch(error => {

console.log(error);

});

}


// Update every 500ms

setInterval(
  updateData,
  500
);

updateData();

</script>

</body>

</html>

)rawliteral";

  return html;
}

// =====================================================
// SEND SENSOR DATA
// =====================================================

void sendData() {

  String json = "{";

  json += "\"ir1\":";
  json += String(ir1State);
  json += ",";

  json += "\"ir2\":";
  json += String(ir2State);
  json += ",";

  json += "\"distance\":";
  json += String(distanceCM, 1);
  json += ",";

  json += "\"motor\":\"";
  json += motorState;
  json += "\",";

  json += "\"status\":\"";
  json += systemStatus;
  json += "\",";

  json += "\"ip\":\"";
  json += WiFi.localIP().toString();
  json += "\"";

  json += "}";


  server.send(
    200,
    "application/json",
    json
  );
}

// =====================================================
// WEB SERVER
// =====================================================

void setupWebServer() {

  server.on("/", []() {

    server.send(
      200,
      "text/html",
      getWebPage()
    );

  });


  server.on(
    "/data",
    sendData
  );


  server.begin();

  Serial.println(
    "Web Server Started"
  );
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  // ===================================================
  // IR SENSOR
  // ===================================================

  pinMode(IR1_PIN, INPUT);
  pinMode(IR2_PIN, INPUT);

  // ===================================================
  // ULTRASONIC
  // ===================================================

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(
    TRIG_PIN,
    LOW
  );

  // ===================================================
  // MOTOR DRIVER
  // ===================================================

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Motor must remain stopped during boot
  stopMotor();

  // ===================================================
  // LCD
  // ===================================================

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  printLCD(
    0,
    "TRACK VEHICLE"
  );

  printLCD(
    1,
    "Starting..."
  );

  delay(1500);

  // ===================================================
  // WIFI
  // ===================================================

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    ssid,
    password
  );

  Serial.println();
  Serial.print(
    "Connecting WiFi"
  );


  printLCD(
    0,
    "Connecting WiFi"
  );

  printLCD(
    1,
    "Please Wait..."
  );


  int attempts = 0;


  while(
    WiFi.status() != WL_CONNECTED &&
    attempts < 40
  )
  {

    delay(500);

    Serial.print(".");

    attempts++;
  }


  if(
    WiFi.status() ==
    WL_CONNECTED
  )
  {

    Serial.println();

    Serial.println(
      "WiFi Connected"
    );


    Serial.print(
      "IP Address: "
    );

    Serial.println(
      WiFi.localIP()
    );


    printLCD(
      0,
      "WiFi Connected"
    );


    printLCD(
      1,
      WiFi.localIP().toString()
    );


    setupWebServer();

    delay(3000);
  }

  else
  {

    Serial.println();

    Serial.println(
      "WiFi Failed"
    );


    printLCD(
      0,
      "WiFi Failed"
    );

    printLCD(
      1,
      "Local Mode"
    );


    delay(2000);
  }
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  // Webpage
  server.handleClient();

  // Main vehicle logic
  controlVehicle();

  // ===================================================
  // SERIAL MONITOR
  // ===================================================

  Serial.print("IR1: ");

  if(ir1State == HIGH)
    Serial.print("HIGH");
  else
    Serial.print("LOW");


  Serial.print(" | IR2: ");

  if(ir2State == HIGH)
    Serial.print("HIGH");
  else
    Serial.print("LOW");


  Serial.print(" | Distance: ");

  Serial.print(
    distanceCM,
    1
  );

  Serial.print(" cm");


  Serial.print(" | Motor: ");

  Serial.print(
    motorState
  );


  Serial.print(" | Status: ");

  Serial.println(
    systemStatus
  );


  delay(100);
}