/*
 * ESP32 + 3.5" 8-bit parallel TFT (Arduino_GFX, ILI9488)
 * Creates Wi-Fi AP, receives JPEG frames from ESP32-CAM over WebSocket,
 * and draws them on screen. No touch: control the rover from the serial
 * monitor (send F, B, L, R, S, X, P).
 */
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <JPEGDEC.h>
using namespace websockets;

// ---------- Wi-Fi (must match the camera sketch) ----------
static const char *AP_SSID     = "rover_tft_01";
static const char *AP_PASSWORD = "rover12345";
static const uint16_t WS_PORT  = 8888;

// ---------- TFT pins (your working test) ----------
#define TFT_RD   26
#define TFT_WR   25
#define TFT_RS   32
#define TFT_CS   33
#define TFT_RST  27
#define TFT_D0   13
#define TFT_D1   14
#define TFT_D2   23
#define TFT_D3   4
#define TFT_D4   18
#define TFT_D5   19
#define TFT_D6   21
#define TFT_D7   22

Arduino_DataBus *bus = new Arduino_ESP32PAR8(
  TFT_RS, TFT_CS, TFT_WR, TFT_RD,
  TFT_D0, TFT_D1, TFT_D2, TFT_D3,
  TFT_D4, TFT_D5, TFT_D6, TFT_D7
);
Arduino_GFX *gfx = new Arduino_ILI9488(bus, TFT_RST, 1, true);

// ---------- Layout (480x320 landscape) ----------
static const int SCREEN_W = 480;
static const int SCREEN_H = 320;
static const int VIDEO_X = 0;
static const int VIDEO_Y = 40;     // 40..280 holds the 320x240 video
static const int VIDEO_W = 320;
static const int VIDEO_H = 240;

WebsocketsServer server;
JPEGDEC jpeg;

static bool viewerConnected = false;
static bool videoPaused = false;
static uint32_t framesReceived = 0;
static uint32_t lastFrameAt = 0;
static uint32_t lastFpsAt = 0;
static uint32_t framesAtLastFps = 0;
static float fps = 0;

// ---------- UI ----------
void drawTop() {
  gfx->fillRect(0, 0, SCREEN_W, 36, 0x0010);  // navy
  gfx->setTextColor(0xFFFF, 0x0010);
  gfx->setTextSize(2);
  gfx->setCursor(8, 10);
  gfx->print("ESP32 CAM LIVE");
  gfx->fillCircle(455, 18, 9, viewerConnected ? 0x07E0 : 0xF800);
}

void drawBottom() {
  gfx->fillRect(0, 284, SCREEN_W, 36, 0x0000);
  gfx->setTextSize(2);
  gfx->setTextColor(0x07FF, 0x0000);
  gfx->setCursor(8, 288);
  if (!viewerConnected) gfx->print("WAITING FOR CAMERA...");
  else gfx->print(videoPaused ? "VIDEO PAUSED" : "LIVE");
  gfx->setTextSize(1);
  gfx->setTextColor(0xC618, 0x0000);
  gfx->setCursor(8, 308);
  gfx->printf("Frames %lu   %.1f fps", (unsigned long)framesReceived, fps);
}

void drawSidePanel() {
  gfx->fillRect(VIDEO_W, 36, SCREEN_W - VIDEO_W, SCREEN_H - 36, 0x0000);
  gfx->setTextSize(2);
  gfx->setTextColor(0xFFFF, 0x0000);
  gfx->setCursor(VIDEO_W + 10, 50);  gfx->print("SERIAL KEYS");
  gfx->setTextColor(0x07E0, 0x0000);
  gfx->setCursor(VIDEO_W + 10, 85);  gfx->print("F  forward");
  gfx->setCursor(VIDEO_W + 10, 110); gfx->print("B  back");
  gfx->setCursor(VIDEO_W + 10, 135); gfx->print("L  left");
  gfx->setCursor(VIDEO_W + 10, 160); gfx->print("R  right");
  gfx->setCursor(VIDEO_W + 10, 185); gfx->print("S  stop");
  gfx->setCursor(VIDEO_W + 10, 210); gfx->print("X  flash");
  gfx->setCursor(VIDEO_W + 10, 235); gfx->print("P  pause");
}

// ---------- JPEG ----------
int jpegDraw(JPEGDRAW *p) {
  gfx->draw16bitRGBBitmap(p->x, p->y, p->pPixels, p->iWidth, p->iHeight);
  return 1;
}

void onFrame(WebsocketsMessage msg) {
  if (!msg.isBinary() || videoPaused) return;
  const uint8_t *data = (const uint8_t *)msg.c_str();
  size_t len = msg.length();
  if (!data || len < 4 || len > 125000) return;
  if (data[0] != 0xFF || data[1] != 0xD8) return;

  if (jpeg.openRAM((uint8_t *)data, (int)len, jpegDraw)) {
    jpeg.decode(80, 40, 0);   // centered: (480-320)/2, (320-240)/2
    jpeg.close();
  }
}

// ---------- Serial -> rover commands ----------
void handleSerial(WebsocketsClient &cam) {
  while (Serial.available()) {
    char c = toupper(Serial.read());
    if (c == 'F' || c == 'B' || c == 'L' || c == 'R' || c == 'S' || c == 'X') {
      cam.send(String(c));
      Serial.printf("[CMD] %c\n", c);
    } else if (c == 'P') {
      videoPaused = !videoPaused;
      cam.send(videoPaused ? "P1" : "P0");
      drawBottom();
      Serial.println(videoPaused ? "[CMD] pause" : "[CMD] resume");
    }
  }
}

// ---------- Setup / loop ----------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32 TFT receiver (Arduino_GFX) ===");

  if (!gfx->begin()) {
    Serial.println("Display init FAILED");
    while (true) delay(1000);
  }
  gfx->setRotation(1);
  gfx->fillScreen(0x0000);
  Serial.printf("[TFT] %d x %d\n", gfx->width(), gfx->height());

  drawTop(); drawBottom(); drawSidePanel();

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.printf("[WiFi] AP %s, IP %s, %s\n", AP_SSID,
                WiFi.softAPIP().toString().c_str(), ok ? "started" : "FAILED");

  server.listen(WS_PORT);
  Serial.printf("[WS] Waiting on ws://192.168.4.1:%u/\n", WS_PORT);
}

void loop() {
  WebsocketsClient cam = server.accept();
  if (cam.available()) {
    Serial.println("[WS] Camera connected!");
    viewerConnected = true;
    videoPaused = false;
    framesReceived = 0;
    framesAtLastFps = 0;
    fps = 0;
    lastFpsAt = millis();
    gfx->fillRect(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H, 0x0000);
    drawTop(); drawBottom();

    cam.onMessage(onFrame);
    while (cam.available()) {
      cam.poll();
      handleSerial(cam);

      uint32_t now = millis();
      if (now - lastFpsAt >= 1000) {
        fps = (framesReceived - framesAtLastFps) * 1000.0f / (now - lastFpsAt);
        framesAtLastFps = framesReceived;
        lastFpsAt = now;
        drawBottom();
      }
      delay(2);
    }
    cam.close();
    viewerConnected = false;
    gfx->fillRect(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H, 0x0000);
    drawTop(); drawBottom();
    Serial.println("[WS] Camera disconnected");
  }
  delay(10);
}