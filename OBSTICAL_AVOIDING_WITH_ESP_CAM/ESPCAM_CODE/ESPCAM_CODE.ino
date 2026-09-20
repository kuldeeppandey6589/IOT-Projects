// #include "esp_camera.h"
// #include <WiFi.h>
// #include <WiFiClientSecure.h>

// // ==========================
// // WiFi Details
// // ==========================
// const char* ssid = "homeiot";
// const char* password = "homeiot123";

// // ==========================
// // Telegram Bot Details
// // =======================
// // Get BOT token from BotFather
// String BOT_TOKEN = "8759495898:AAFJRgeUPDplnu8dhUUdVqdcUozwPuyhEG4";

// // Get chat id from @userinfobot or @myidbot
// String CHAT_ID = "1557169121";
// // ==========================
// // ESP32-CAM Pins
// // ==========================
// #define ARDUINO_TRIGGER_PIN 13
// #define LDR_PIN 14
// #define FLASH_LED_PIN 4

// // ==========================
// // AI Thinker ESP32-CAM Camera Pins
// // ==========================
// #define PWDN_GPIO_NUM     32
// #define RESET_GPIO_NUM    -1
// #define XCLK_GPIO_NUM      0
// #define SIOD_GPIO_NUM     26
// #define SIOC_GPIO_NUM     27

// #define Y9_GPIO_NUM       35
// #define Y8_GPIO_NUM       34
// #define Y7_GPIO_NUM       39
// #define Y6_GPIO_NUM       36
// #define Y5_GPIO_NUM       21
// #define Y4_GPIO_NUM       19
// #define Y3_GPIO_NUM       18
// #define Y2_GPIO_NUM        5

// #define VSYNC_GPIO_NUM    25
// #define HREF_GPIO_NUM     23
// #define PCLK_GPIO_NUM     22

// bool lastTriggerState = LOW;

// unsigned long lastPhotoTime = 0;
// const unsigned long photoDelay = 10000; // 10 seconds gap

// WiFiClientSecure client;

// void setup() {
//   Serial.begin(115200);

//   pinMode(ARDUINO_TRIGGER_PIN, INPUT);
//   pinMode(LDR_PIN, INPUT);
//   pinMode(FLASH_LED_PIN, OUTPUT);

//   digitalWrite(FLASH_LED_PIN, LOW);

//   connectWiFi();
//   setupCamera();

//   client.setInsecure();

//   Serial.println("ESP32-CAM Ready");
// }

// void loop() {
//   bool triggerState = digitalRead(ARDUINO_TRIGGER_PIN);

//   if (triggerState == HIGH && lastTriggerState == LOW) {
//     if (millis() - lastPhotoTime >= photoDelay) {
//       lastPhotoTime = millis();

//       Serial.println("Trigger received from Arduino");
//       captureAndSendPhoto();
//     }
//   }

//   lastTriggerState = triggerState;

//   delay(50);
// }

// void connectWiFi() {
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(ssid, password);

//   Serial.print("Connecting to WiFi");

//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }

//   Serial.println();
//   Serial.println("WiFi connected");
//   Serial.print("IP Address: ");
//   Serial.println(WiFi.localIP());
// }

// void setupCamera() {
//   camera_config_t config;

//   config.ledc_channel = LEDC_CHANNEL_0;
//   config.ledc_timer = LEDC_TIMER_0;

//   config.pin_d0 = Y2_GPIO_NUM;
//   config.pin_d1 = Y3_GPIO_NUM;
//   config.pin_d2 = Y4_GPIO_NUM;
//   config.pin_d3 = Y5_GPIO_NUM;
//   config.pin_d4 = Y6_GPIO_NUM;
//   config.pin_d5 = Y7_GPIO_NUM;
//   config.pin_d6 = Y8_GPIO_NUM;
//   config.pin_d7 = Y9_GPIO_NUM;

//   config.pin_xclk = XCLK_GPIO_NUM;
//   config.pin_pclk = PCLK_GPIO_NUM;
//   config.pin_vsync = VSYNC_GPIO_NUM;
//   config.pin_href = HREF_GPIO_NUM;

//   config.pin_sscb_sda = SIOD_GPIO_NUM;
//   config.pin_sscb_scl = SIOC_GPIO_NUM;

//   config.pin_pwdn = PWDN_GPIO_NUM;
//   config.pin_reset = RESET_GPIO_NUM;

//   config.xclk_freq_hz = 20000000;
//   config.pixel_format = PIXFORMAT_JPEG;

//   if (psramFound()) {
//     config.frame_size = FRAMESIZE_VGA;
//     config.jpeg_quality = 10;
//     config.fb_count = 2;
//   } else {
//     config.frame_size = FRAMESIZE_QVGA;
//     config.jpeg_quality = 12;
//     config.fb_count = 1;
//   }

//   esp_err_t err = esp_camera_init(&config);

//   if (err != ESP_OK) {
//     Serial.printf("Camera init failed with error 0x%x", err);
//     delay(1000);
//     ESP.restart();
//   }

//   Serial.println("Camera initialized");
// }

// void captureAndSendPhoto() {
//   int ldrState = digitalRead(LDR_PIN);

//   // For most LDR modules:
//   // LOW = dark
//   // HIGH = light
//   bool isDark = (ldrState == LOW);

//   if (isDark) {
//     Serial.println("Dark detected. Flash ON");
//     digitalWrite(FLASH_LED_PIN, HIGH);
//     delay(700);
//   } else {
//     Serial.println("Enough light. Flash OFF");
//     digitalWrite(FLASH_LED_PIN, LOW);
//     delay(200);
//   }

//   camera_fb_t* fb = esp_camera_fb_get();

//   if (!fb) {
//     Serial.println("Camera capture failed");
//     digitalWrite(FLASH_LED_PIN, LOW);
//     return;
//   }

//   Serial.println("Photo captured");
//   sendPhotoToTelegram(fb);

//   esp_camera_fb_return(fb);

//   digitalWrite(FLASH_LED_PIN, LOW);
// }

// void sendPhotoToTelegram(camera_fb_t* fb) {
//   Serial.println("Connecting to Telegram...");

//   if (!client.connect("api.telegram.org", 443)) {
//     Serial.println("Telegram connection failed");
//     return;
//   }

//   String boundary = "----ESP32CAMBoundary";
//   String head = "--" + boundary + "\r\n";
//   head += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
//   head += CHAT_ID + "\r\n";
//   head += "--" + boundary + "\r\n";
//   head += "Content-Disposition: form-data; name=\"photo\"; filename=\"esp32cam.jpg\"\r\n";
//   head += "Content-Type: image/jpeg\r\n\r\n";

//   String tail = "\r\n--" + boundary + "--\r\n";

//   uint32_t imageLen = fb->len;
//   uint32_t totalLen = head.length() + imageLen + tail.length();

//   String request = "POST /bot" + BOT_TOKEN + "/sendPhoto HTTP/1.1\r\n";
//   request += "Host: api.telegram.org\r\n";
//   request += "Content-Length: " + String(totalLen) + "\r\n";
//   request += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
//   request += "Connection: close\r\n\r\n";

//   client.print(request);
//   client.print(head);

//   uint8_t* fbBuf = fb->buf;
//   size_t fbLen = fb->len;

//   for (size_t n = 0; n < fbLen; n = n + 1024) {
//     if (n + 1024 < fbLen) {
//       client.write(fbBuf, 1024);
//       fbBuf += 1024;
//     } else {
//       size_t remainder = fbLen % 1024;
//       client.write(fbBuf, remainder);
//     }
//   }

//   client.print(tail);

//   Serial.println("Photo sent. Telegram response:");

//   long timeout = millis() + 10000;

//   while (client.connected() && millis() < timeout) {
//     while (client.available()) {
//       String line = client.readStringUntil('\n');
//       Serial.println(line);
//     }
//   }

//   client.stop();
// }



#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

// ==========================
// WiFi Details
// ==========================
const char* ssid = "homeiot";
const char* password = "homeiot123";

// ==========================
// Telegram Bot Details
// ==========================
// IMPORTANT: Regenerate your bot token from BotFather if this code was shared publicly
String BOT_TOKEN = "8759495898:AAFJRgeUPDplnu8dhUUdVqdcUozwPuyhEG4";

// Telegram Chat IDs
String CHAT_ID_1 = "1557169121";
String CHAT_ID_2 = "6392514213";   

// ==========================
// ESP32-CAM Pins
// ==========================
#define ARDUINO_TRIGGER_PIN 13
#define LDR_PIN 14
#define FLASH_LED_PIN 4

// ==========================
// AI Thinker ESP32-CAM Camera Pins
// ==========================
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

bool lastTriggerState = LOW;

unsigned long lastPhotoTime = 0;
const unsigned long photoDelay = 10000; // 10 seconds gap

WiFiClientSecure client;

void setup() {
  Serial.begin(115200);

  pinMode(ARDUINO_TRIGGER_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(FLASH_LED_PIN, OUTPUT);

  digitalWrite(FLASH_LED_PIN, LOW);

  connectWiFi();
  setupCamera();

  client.setInsecure();

  Serial.println("ESP32-CAM Ready");
}

void loop() {
  bool triggerState = digitalRead(ARDUINO_TRIGGER_PIN);

  if (triggerState == HIGH && lastTriggerState == LOW) {
    if (millis() - lastPhotoTime >= photoDelay) {
      lastPhotoTime = millis();

      Serial.println("Trigger received from Arduino");
      captureAndSendPhoto();
    }
  }

  lastTriggerState = triggerState;

  delay(50);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void setupCamera() {
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
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  Serial.println("Camera initialized");
}

void captureAndSendPhoto() {
  int ldrState = digitalRead(LDR_PIN);

  // For most LDR modules:
  // LOW = dark
  // HIGH = light
  bool isDark = (ldrState == HIGH);

  if (isDark) {
    Serial.println("Dark detected. Flash ON");
    digitalWrite(FLASH_LED_PIN, LOW);
    delay(700);
  } else {
    Serial.println("Enough light. Flash OFF");
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(200);
  }

  camera_fb_t* fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Camera capture failed");
    digitalWrite(FLASH_LED_PIN, LOW);
    return;
  }

  Serial.println("Photo captured");

  // Send same photo to both Telegram accounts
  sendPhotoToTelegram(fb, CHAT_ID_1);
  delay(1000);
  sendPhotoToTelegram(fb, CHAT_ID_2);

  esp_camera_fb_return(fb);

  digitalWrite(FLASH_LED_PIN, LOW);
}

void sendPhotoToTelegram(camera_fb_t* fb, String chatId) {
  if (chatId == "" || chatId == "SECOND_ACCOUNT_CHAT_ID") {
    Serial.println("Invalid chat ID. Skipping...");
    return;
  }

  Serial.print("Sending photo to chat ID: ");
  Serial.println(chatId);

  Serial.println("Connecting to Telegram...");

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("Telegram connection failed");
    return;
  }

  String boundary = "----ESP32CAMBoundary";

  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
  head += chatId + "\r\n";
  head += "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"photo\"; filename=\"esp32cam.jpg\"\r\n";
  head += "Content-Type: image/jpeg\r\n\r\n";

  String tail = "\r\n--" + boundary + "--\r\n";

  uint32_t imageLen = fb->len;
  uint32_t totalLen = head.length() + imageLen + tail.length();

  String request = "POST /bot" + BOT_TOKEN + "/sendPhoto HTTP/1.1\r\n";
  request += "Host: api.telegram.org\r\n";
  request += "Content-Length: " + String(totalLen) + "\r\n";
  request += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
  request += "Connection: close\r\n\r\n";

  client.print(request);
  client.print(head);

  uint8_t* fbBuf = fb->buf;
  size_t fbLen = fb->len;

  for (size_t n = 0; n < fbLen; n += 1024) {
    if (n + 1024 < fbLen) {
      client.write(fbBuf, 1024);
      fbBuf += 1024;
    } else {
      size_t remainder = fbLen - n;
      client.write(fbBuf, remainder);
    }
  }

  client.print(tail);

  Serial.println("Photo sent. Telegram response:");

  long timeout = millis() + 10000;

  while (client.connected() && millis() < timeout) {
    while (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    }
  }

  client.stop();
}