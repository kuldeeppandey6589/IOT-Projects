#define BLYNK_TEMPLATE_ID "TMPL34X35BA8d"
#define BLYNK_TEMPLATE_NAME "Fall Detection"
#define BLYNK_AUTH_TOKEN "3jc69TBnWBOpyA-1VexifG5SZ1RSSXrB"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =======================
// WiFi Details
// =======================
char ssid[] = "homeiot";
char pass[] = "homeiot123";

// =======================
// LCD
// =======================
// Try 0x27 first. If LCD not working, change to 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =======================
// MPU6050 Address
// =======================
#define MPU_ADDR 0x68

// =======================
// MPU6050 Raw Data
// =======================
int16_t rawAx, rawAy, rawAz;
float ax_g = 0;
float ay_g = 0;
float az_g = 0;
float accelerationMagnitude = 0;

// =======================
// Fall Detection Settings
// =======================
const float LOWER_THRESHOLD = 0.5;
const float UPPER_THRESHOLD = 2.8;

bool mpuReady = false;
bool possibleFall = false;
bool fallDetected = false;

unsigned long fallStartTime = 0;
unsigned long lastAlertTime = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastSerialUpdate = 0;
unsigned long lastMPURetry = 0;

const unsigned long FALL_TIME_LIMIT = 1500;
const unsigned long ALERT_COOLDOWN = 10000;
const unsigned long LCD_UPDATE_TIME = 500;
const unsigned long SERIAL_UPDATE_TIME = 1000;
const unsigned long MPU_RETRY_TIME = 5000;

// =======================
// Function Declarations
// =======================
void connectWiFi();
void connectBlynk();
bool checkMPU6050();
void setupMPU6050();
void readMPU6050();
void detectFall();
void updateLCD();
void updateSerialMonitor();
void writeMPU(byte reg, byte data);
byte readMPU(byte reg);

// =======================
// Setup
// =======================
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("====================================");
  Serial.println("ESP8266 FALL DETECTION SYSTEM");
  Serial.println("MPU6050 RAW I2C + LCD + BLYNK");
  Serial.println("====================================");

  // ESP8266 I2C pins
  // D2 = SDA, D1 = SCL
  Wire.begin(D2, D1);
  Wire.setClock(100000);   // 100kHz stable I2C speed

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Fall Detection");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(1500);

  setupMPU6050();
  connectWiFi();
  connectBlynk();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  lcd.setCursor(0, 1);
  lcd.print("Monitoring...");

  Serial.println("SYSTEM READY");
}

// =======================
// Loop
// =======================
void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    connectWiFi();
    connectBlynk();
  }

  if (!mpuReady) {
    if (millis() - lastMPURetry >= MPU_RETRY_TIME) {
      lastMPURetry = millis();
      setupMPU6050();
    }
  }

  if (mpuReady) {
    readMPU6050();
    detectFall();
  }

  updateSerialMonitor();
  updateLCD();

  delay(100);
}

// =======================
// MPU6050 Register Write
// =======================
void writeMPU(byte reg, byte data) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

// =======================
// MPU6050 Register Read
// =======================
byte readMPU(byte reg) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 1);

  if (Wire.available()) {
    return Wire.read();
  }

  return 0;
}

// =======================
// Check MPU6050
// =======================
bool checkMPU6050() {
  Wire.beginTransmission(MPU_ADDR);
  byte error = Wire.endTransmission();

  if (error == 0) {
    return true;
  } else {
    return false;
  }
}

// =======================
// Setup MPU6050
// =======================
void setupMPU6050() {
  Serial.println("Checking MPU6050 at 0x68...");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Checking");
  lcd.setCursor(0, 1);
  lcd.print("MPU6050 0x68");

  if (!checkMPU6050()) {
    Serial.println("MPU6050 Not Found on I2C!");
    mpuReady = false;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MPU6050 Error");
    lcd.setCursor(0, 1);
    lcd.print("Check Wiring");
    delay(1500);
    return;
  }

  Serial.println("MPU6050 Found on I2C");

  // Wake up MPU6050
  writeMPU(0x6B, 0x00);
  delay(100);

  // Accelerometer range: +/- 2g
  // Scale factor = 16384 LSB/g
  writeMPU(0x1C, 0x00);

  // Gyro range: +/- 250 deg/s
  writeMPU(0x1B, 0x00);

  // Low pass filter
  writeMPU(0x1A, 0x03);

  byte whoAmI = readMPU(0x75);

  Serial.print("WHO_AM_I Register: 0x");
  Serial.println(whoAmI, HEX);

  if (whoAmI == 0x68 || whoAmI == 0x70 || whoAmI == 0x71) {
    mpuReady = true;

    Serial.println("MPU6050 Ready!");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MPU6050 Ready");
    lcd.setCursor(0, 1);
    lcd.print("Address 0x68");
    delay(1500);
  } else {
    // Some clone modules may return unexpected WHO_AM_I.
    // But since I2C is detected, still allow reading.
    mpuReady = true;

    Serial.println("MPU detected, WHO_AM_I unusual");
    Serial.println("Continuing anyway...");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MPU Detected");
    lcd.setCursor(0, 1);
    lcd.print("Continuing...");
    delay(1500);
  }
}

// =======================
// Read MPU6050
// =======================
void readMPU6050() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);  // Accelerometer data start register
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6);

  if (Wire.available() == 6) {
    rawAx = Wire.read() << 8 | Wire.read();
    rawAy = Wire.read() << 8 | Wire.read();
    rawAz = Wire.read() << 8 | Wire.read();

    // Because accelerometer range is +/- 2g
    ax_g = rawAx / 16384.0;
    ay_g = rawAy / 16384.0;
    az_g = rawAz / 16384.0;

    accelerationMagnitude = sqrt((ax_g * ax_g) + (ay_g * ay_g) + (az_g * az_g));

    if (Blynk.connected()) {
      Blynk.virtualWrite(V0, accelerationMagnitude);
    }
  } else {
    Serial.println("MPU6050 read failed");
    mpuReady = false;
  }
}

// =======================
// Connect WiFi
// =======================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(1000);

  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connecting");
  lcd.setCursor(0, 1);
  lcd.print(ssid);

  WiFi.begin(ssid, pass);

  int count = 0;

  while (WiFi.status() != WL_CONNECTED && count < 40) {
    delay(500);
    Serial.print(".");
    count++;

    lcd.setCursor(0, 1);
    lcd.print("Trying: ");
    lcd.print(count);
    lcd.print("      ");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected Successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(2000);
  } else {
    Serial.println("WiFi Not Connected!");
    Serial.print("WiFi Status Code: ");
    Serial.println(WiFi.status());

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed");
    lcd.setCursor(0, 1);
    lcd.print("Check Hotspot");
    delay(2000);
  }
}

// =======================
// Connect Blynk
// =======================
void connectBlynk() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Blynk skipped. WiFi not connected.");
    return;
  }

  Serial.println("Connecting to Blynk...");

  Blynk.config(BLYNK_AUTH_TOKEN);
}

// =======================
// Fall Detection
// =======================
void detectFall() {
  unsigned long currentTime = millis();

  // Step 1: Free-fall condition
  if (accelerationMagnitude < LOWER_THRESHOLD && !possibleFall) {
    possibleFall = true;
    fallStartTime = currentTime;

    Serial.println("Possible Fall Started");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Possible Fall");
    lcd.setCursor(0, 1);
    lcd.print("Checking...");
  }

  // Step 2: Impact condition after free fall
  if (possibleFall && accelerationMagnitude > UPPER_THRESHOLD) {
    fallDetected = true;
    possibleFall = false;

    Serial.println("********************************");
    Serial.println("FALL DETECTED!");
    Serial.println("********************************");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("FALL DETECTED!");
    lcd.setCursor(0, 1);
    lcd.print("Sending Alert");

    if (Blynk.connected()) {
      if (currentTime - lastAlertTime > ALERT_COOLDOWN) {
        Blynk.logEvent("fall_alert", "Fall detected! Please check immediately.");
        lastAlertTime = currentTime;

        Serial.println("Blynk Alert Sent");

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("FALL DETECTED!");
        lcd.setCursor(0, 1);
        lcd.print("Alert Sent");
      }

      Blynk.virtualWrite(V1, 1);
    } else {
      Serial.println("Blynk Offline. Alert not sent.");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Fall Detected");
      lcd.setCursor(0, 1);
      lcd.print("Blynk Offline");
    }

    delay(3000);

    fallDetected = false;

    if (Blynk.connected()) {
      Blynk.virtualWrite(V1, 0);
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("System Ready");
    lcd.setCursor(0, 1);
    lcd.print("Monitoring...");
  }

  // Step 3: False alarm reset
  if (possibleFall && currentTime - fallStartTime > FALL_TIME_LIMIT) {
    possibleFall = false;

    Serial.println("False Alarm");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("False Alarm");
    lcd.setCursor(0, 1);
    lcd.print("No Impact");
    delay(1000);
  }
}

// =======================
// Serial Monitor
// =======================
void updateSerialMonitor() {
  unsigned long currentTime = millis();

  if (currentTime - lastSerialUpdate >= SERIAL_UPDATE_TIME) {
    lastSerialUpdate = currentTime;

    Serial.println("--------------------------------");

    Serial.print("WiFi: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");

    Serial.print("Blynk: ");
    Serial.println(Blynk.connected() ? "Connected" : "Disconnected");

    Serial.print("MPU6050: ");
    Serial.println(mpuReady ? "Connected" : "Not Found");

    if (mpuReady) {
      Serial.print("X: ");
      Serial.print(ax_g, 2);
      Serial.println(" g");

      Serial.print("Y: ");
      Serial.print(ay_g, 2);
      Serial.println(" g");

      Serial.print("Z: ");
      Serial.print(az_g, 2);
      Serial.println(" g");

      Serial.print("Total Acc: ");
      Serial.print(accelerationMagnitude, 2);
      Serial.println(" g");
    }

    Serial.print("Fall Status: ");
    if (fallDetected) {
      Serial.println("FALL DETECTED");
    } else if (possibleFall) {
      Serial.println("Possible Fall");
    } else {
      Serial.println("Normal");
    }
  }
}

// =======================
// LCD Display
// =======================
void updateLCD() {
  unsigned long currentTime = millis();

  if (currentTime - lastLCDUpdate >= LCD_UPDATE_TIME) {
    lastLCDUpdate = currentTime;

    if (!mpuReady) {
      lcd.setCursor(0, 0);
      lcd.print("MPU6050 Error  ");
      lcd.setCursor(0, 1);
      lcd.print("Check Wiring   ");
      return;
    }

    if (!possibleFall && !fallDetected) {
      lcd.setCursor(0, 0);
      lcd.print("Monitoring...  ");

      lcd.setCursor(0, 1);
      lcd.print("Acc:");
      lcd.print(accelerationMagnitude, 2);
      lcd.print("g     ");
    }
  }
}