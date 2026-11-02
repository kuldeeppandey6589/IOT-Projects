/*
 * ESP32-CAM (RHYX M21-45 / GC2145) -> Wi-Fi -> WebSocket -> ESP32 TFT display
 * RGB565 capture -> software JPEG -> WebSocket binary frame.
 *
 * Commands received from the display: F B L R S (drive/stop),
 * X (flash toggle), P1 (pause video), P0 (resume video).
 *
 * Board: AI Thinker ESP32-CAM, PSRAM enabled, 115200 baud.
 * Motors need their own supply, with common GND to the ESP32-CAM.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include "esp_camera.h"
#include "img_converters.h"

using namespace websockets;

// ---------- Must match the display sketch ----------
static const char* AP_SSID = "homeiot";
static const char* AP_PASSWORD = "homeiot123";
static const char* WEBSOCKET_URL = "ws://192.168.4.1:8888/";

// ---------- Stream settings ----------
static const uint32_t FRAME_INTERVAL_MS = 250;  // ~4 fps max
static const int SOFTWARE_JPEG_QUALITY = 85;    // 0..100
static const uint32_t CAMERA_CLOCK_HZ = 20000000;
static const framesize_t CAMERA_FRAME_SIZE = FRAMESIZE_QVGA;  // 320x240
static const uint32_t MOTOR_TIMEOUT_MS = 800;

// ---------- Rover motor pins (L298N) ----------
#define IN1 12
#define IN2 13
#define IN3 15
#define IN4 14
#define WHITE_FLASH_PIN 4

// ---------- AI Thinker ESP32-CAM pinout ----------
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

WebsocketsClient websocket;
static bool socketConnected = false;
static bool cameraReady = false;
static bool streamPaused = false;
static bool flashOn = false;
static char activeDirection = 'S';
static uint32_t lastDriveCommandMs = 0;
static uint32_t lastFrameMs = 0;
static uint32_t lastReconnectAttemptMs = 0;
static uint32_t lastWiFiAttemptMs = 0;
static uint32_t framesSent = 0;
static uint32_t lastStatMs = 0;

// ---------- Motors ----------
void motorStop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  activeDirection = 'S';
}

void setDrive(char direction) {
  lastDriveCommandMs = millis();
  if (direction == activeDirection) return;  // heartbeat, no change
  motorStop();
  switch (direction) {
    case 'F':
      digitalWrite(IN1, HIGH);
      digitalWrite(IN3, HIGH);
      break;
    case 'B':
      digitalWrite(IN2, HIGH);
      digitalWrite(IN4, HIGH);
      break;
    case 'L':
      digitalWrite(IN2, HIGH);
      digitalWrite(IN3, HIGH);
      break;
    case 'R':
      digitalWrite(IN1, HIGH);
      digitalWrite(IN4, HIGH);
      break;
    default: return;
  }
  activeDirection = direction;
}

void processCommand(const String& command) {
  if (command == "F" || command == "B" || command == "L" || command == "R") {
    setDrive(command.charAt(0));
  } else if (command == "S") {
    motorStop();
  } else if (command == "X") {
    flashOn = !flashOn;
    digitalWrite(WHITE_FLASH_PIN, flashOn ? HIGH : LOW);
    Serial.printf("[FLASH] %s\n", flashOn ? "ON" : "OFF");
  } else if (command == "P1") {
    streamPaused = true;
    Serial.println("[STREAM] paused");
  } else if (command == "P0") {
    streamPaused = false;
    Serial.println("[STREAM] resumed");
  }
}

// ---------- Camera ----------
bool cameraInit() {
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM;
  c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM;
  c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM;
  c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM;
  c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM;
  c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM;
  c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM;
  c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = CAMERA_CLOCK_HZ;

  c.pixel_format = PIXFORMAT_RGB565;  // GC2145 has no hardware JPEG
  c.frame_size = psramFound() ? CAMERA_FRAME_SIZE : FRAMESIZE_QQVGA;
  c.jpeg_quality = 12;  // ignored for RGB565
  c.fb_count = 1;
  c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  c.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;

  Serial.println(psramFound() ? "[CAM] PSRAM detected"
                              : "[CAM] PSRAM NOT detected (enable PSRAM in Tools menu)");

  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    Serial.printf("[CAM] init FAILED: 0x%X\n", (unsigned)err);
    return false;
  }
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
  s->set_brightness(s, 0);
  s->set_contrast(s, 1);      // -2..2, more contrast fixes the dull look
  s->set_saturation(s, 1);    // -2..2, more color
  s->set_sharpness(s, 1);
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  s->set_exposure_ctrl(s, 1);
  s->set_gain_ctrl(s, 1);
  s->set_lenc(s, 1);
}
}

// ---------- WebSocket events ----------
void onWebsocketEvent(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionOpened) {
    socketConnected = true;
    streamPaused = false;
    Serial.println("[WS] Connected to TFT display");
  } else if (event == WebsocketsEvent::ConnectionClosed) {
    socketConnected = false;
    motorStop();  // fail-safe
    Serial.println("[WS] Disconnected; motors stopped");
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("\n=== ESP32-CAM -> TFT live video ===");

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(WHITE_FLASH_PIN, OUTPUT);
  motorStop();
  digitalWrite(WHITE_FLASH_PIN, LOW);

  cameraReady = cameraInit();

  WiFi.persistent(false);
  WiFi.disconnect(true, true);  // erase saved credentials (use once)
  delay(200);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin("rover_tft_01", "rover12345");

  websocket.onMessage([](WebsocketsMessage msg) {
    if (msg.isText()) processCommand(msg.data());
  });
  websocket.onEvent(onWebsocketEvent);

  Serial.printf("[Wi-Fi] Joining AP: %s\n", AP_SSID);
}

// ---------- Loop ----------
void loop() {
  uint32_t now = millis();

  // Safety watchdog: no repeated command => stop
  if (activeDirection != 'S' && (uint32_t)(now - lastDriveCommandMs) > MOTOR_TIMEOUT_MS) {
    motorStop();
  }

  // --- Wi-Fi ---
  if (WiFi.status() != WL_CONNECTED) {
    if (socketConnected) {
      websocket.close();
      socketConnected = false;
    }
    motorStop();
    if ((uint32_t)(now - lastWiFiAttemptMs) > 10000) {
      lastWiFiAttemptMs = now;
      Serial.println("[Wi-Fi] Retrying...");
      WiFi.disconnect();
      delay(100);
      WiFi.begin(AP_SSID, AP_PASSWORD);
    }
    delay(30);
    return;
  }

  // --- WebSocket connect ---
  if (!socketConnected) {
    if ((uint32_t)(now - lastReconnectAttemptMs) > 2500) {
      lastReconnectAttemptMs = now;
      Serial.printf("[Wi-Fi] IP %s, gateway %s\n",
                    WiFi.localIP().toString().c_str(),
                    WiFi.gatewayIP().toString().c_str());
      Serial.printf("[WS] Connecting to %s\n", WEBSOCKET_URL);
      socketConnected = websocket.connect(WEBSOCKET_URL);
      if (!socketConnected) Serial.println("[WS] No display server yet");
    }
    delay(15);
    return;
  }

  // --- Receive commands ---
  websocket.poll();
  if (!socketConnected || !cameraReady || streamPaused) {
    delay(5);
    return;
  }

  // --- Frame timing ---
  now = millis();
  if ((uint32_t)(now - lastFrameMs) < FRAME_INTERVAL_MS) {
    delay(2);
    return;
  }
  lastFrameMs = now;

  // --- Capture + encode + send ---
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[CAM] Capture failed");
    return;
  }

  uint8_t* jpegBuffer = nullptr;
  size_t jpegLength = 0;
  bool converted = frame2jpg(fb, SOFTWARE_JPEG_QUALITY, &jpegBuffer, &jpegLength);
  esp_camera_fb_return(fb);

  if (!converted || jpegBuffer == nullptr || jpegLength == 0) {
    Serial.println("[CAM] JPEG conversion failed");
    if (jpegBuffer) free(jpegBuffer);
    delay(20);
    return;
  }

  bool sent = websocket.sendBinary((const char*)jpegBuffer, jpegLength);
  free(jpegBuffer);
  if (sent) framesSent++;
  else Serial.println("[WS] Frame send failed");

  if (millis() - lastStatMs > 5000) {
    lastStatMs = millis();
    Serial.printf("[STAT] frames sent=%lu, last JPEG=%u bytes\n",
                  (unsigned long)framesSent, (unsigned)jpegLength);
  }

  websocket.poll();
}