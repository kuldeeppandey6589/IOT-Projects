/*************************************************
 ESP32-CAM + HC-SR04 + Telegram Bot
 When object comes closer than 30 cm,
 ESP32-CAM captures image and sends to Telegram
 with distance/details.
*************************************************/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"

// =======================
// WiFi Details
// =======================
const char* ssid = "homeiot";
const char* password = "homeiot123";

// =======================
// Telegram Bot Details
// =======================
// Get BOT token from BotFather
String BOT_TOKEN = "8759495898:AAFJRgeUPDplnu8dhUUdVqdcUozwPuyhEG4";

// Get chat id from @userinfobot or @myidbot
String CHAT_ID = "1557169121";

// =======================
// Ultrasonic Pins
// =======================
#define TRIG_PIN 13
#define ECHO_PIN 12

// Object detection range in cm
#define DETECT_DISTANCE 25

// Delay between photo sending
unsigned long lastPhotoTime = 0;
const unsigned long photoDelay = 10000;   // 10 seconds gap

WiFiClientSecure client;

// =======================
// ESP32-CAM AI Thinker Pins
// =======================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// =======================
// Camera Setup
// =======================
bool setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  return true;
}

// =======================
// Read Distance
// =======================
float getDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    return -1;
  }

  float distance = duration * 0.034 / 2;
  return distance;
}

// =======================
// Send Photo to Telegram with Details
// =======================
bool sendPhotoTelegram(float distance) {
  const char* telegramServer = "api.telegram.org";

  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Camera capture failed");
    return false;
  }

  String caption = "Object Detected!\n";
  caption += "Distance: ";
  caption += String(distance, 1);
  caption += " cm\n";
  caption += "Alert Range: Less than 30 cm\n";
  caption += "Device: ESP32-CAM Security System\n";
  caption += "IP Address: ";
  caption += WiFi.localIP().toString();

  Serial.println("Connecting to Telegram...");

  if (!client.connect(telegramServer, 443)) {
    Serial.println("Telegram connection failed");
    esp_camera_fb_return(fb);
    return false;
  }

  String head = "--ESP32CAM\r\n";
  head += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
  head += CHAT_ID;

  head += "\r\n--ESP32CAM\r\n";
  head += "Content-Disposition: form-data; name=\"caption\"\r\n\r\n";
  head += caption;

  head += "\r\n--ESP32CAM\r\n";
  head += "Content-Disposition: form-data; name=\"photo\"; filename=\"esp32cam.jpg\"\r\n";
  head += "Content-Type: image/jpeg\r\n\r\n";

  String tail = "\r\n--ESP32CAM--\r\n";

  uint32_t imageLen = fb->len;
  uint32_t totalLen = head.length() + imageLen + tail.length();

  String request = "POST /bot" + BOT_TOKEN + "/sendPhoto HTTP/1.1\r\n";
  request += "Host: api.telegram.org\r\n";
  request += "Content-Length: " + String(totalLen) + "\r\n";
  request += "Content-Type: multipart/form-data; boundary=ESP32CAM\r\n\r\n";

  client.print(request);
  client.print(head);

  uint8_t *fbBuf = fb->buf;
  size_t fbLen = fb->len;

  for (size_t n = 0; n < fbLen; n += 1024) {
    size_t bytesToWrite = min((size_t)1024, fbLen - n);
    client.write(fbBuf, bytesToWrite);
    fbBuf += bytesToWrite;
  }

  client.print(tail);

  esp_camera_fb_return(fb);

  Serial.println("Photo with details sent to Telegram");
  return true;
}

// =======================
// Setup
// =======================
void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.println();
  Serial.println("ESP32-CAM Telegram Security System");

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  client.setInsecure();

  if (setupCamera()) {
    Serial.println("Camera Ready");
  } else {
    Serial.println("Camera Failed");
  }
}

// =======================
// Loop
// =======================
void loop() {
  float distance = getDistanceCM();

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance > 0 && distance < DETECT_DISTANCE) {
    Serial.println("Object less than 30 cm detected!");

    if (millis() - lastPhotoTime > photoDelay) {
      lastPhotoTime = millis();
      sendPhotoTelegram(distance);
    }
  }

  delay(500);
}