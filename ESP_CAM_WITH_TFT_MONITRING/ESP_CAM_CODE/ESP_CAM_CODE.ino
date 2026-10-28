#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"

// AI Thinker ESP32-CAM camera pins
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

const char *apSSID = "Robotutor";
const char *apPassword = "Robotutor";

WiFiServer server(80);

bool startCamera() {
  camera_config_t config = {};

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

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Start with 320 x 240 for better speed
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 18;
  config.fb_count = 1;

  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t result = esp_camera_init(&config);

  if (result != ESP_OK) {
    Serial.printf(
      "Camera failed: 0x%x\n", result
    );
    return false;
  }

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Starting ESP32-CAM...");

  if (!startCamera()) {
    Serial.println("Check camera connection!");
    while (true) {
      delay(1000);
    }
  }

  WiFi.mode(WIFI_AP);

  if (!WiFi.softAP(apSSID, apPassword)) {
    Serial.println("WiFi AP failed!");
    while (true) delay(1000);
  }

  WiFi.setSleep(false);
  server.begin();

  Serial.println("Camera is ready!");
  Serial.print("WiFi: ");
  Serial.println(apSSID);

  Serial.print("Camera URL: http://");
  Serial.print(WiFi.softAPIP());
  Serial.println("/jpg");
}

void loop() {
  WiFiClient client = server.available();

  if (!client) {
    delay(2);
    return;
  }

  client.setTimeout(1500);

  String request = client.readStringUntil('\n');
  request.trim();

  // Consume remaining HTTP headers
  unsigned long start = millis();

  while (client.connected() &&
         millis() - start < 1000) {
    if (!client.available()) {
      delay(1);
      continue;
    }

    String line = client.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) break;
  }

  if (!request.startsWith("GET /jpg ") &&
      !request.startsWith("GET /capture ")) {
    client.print(
      "HTTP/1.1 404 Not Found\r\n"
      "Content-Length: 0\r\n"
      "Connection: close\r\n\r\n"
    );
    client.stop();
    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Capture failed");

    client.print(
      "HTTP/1.1 503 Service Unavailable\r\n"
      "Content-Length: 0\r\n"
      "Connection: close\r\n\r\n"
    );

    client.stop();
    return;
  }

  client.printf(
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: image/jpeg\r\n"
    "Content-Length: %u\r\n"
    "Cache-Control: no-store\r\n"
    "Connection: close\r\n\r\n",
    (unsigned int)fb->len
  );

  size_t sent = client.write(fb->buf, fb->len);

  Serial.printf(
    "Frame: %u bytes, sent: %u\n",
    (unsigned int)fb->len,
    (unsigned int)sent
  );

  esp_camera_fb_return(fb);

  client.flush();
  client.stop();
}
