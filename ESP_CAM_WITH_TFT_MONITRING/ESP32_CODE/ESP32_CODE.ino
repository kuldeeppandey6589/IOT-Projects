
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_GFX_Library.h>
#include <JPEGDEC.h>

// WiFi from ESP32-CAM
const char *ssid = "Robotutor";
const char *password = "Robotutor";

const char *cameraURL =
    "http://192.168.4.1/jpg";

// TFT control pins
#define TFT_RD   26
#define TFT_WR   25
#define TFT_RS   32
#define TFT_CS   33
#define TFT_RST  27

// TFT parallel data pins
#define TFT_D0   13
#define TFT_D1   14
#define TFT_D2   23
#define TFT_D3   4
#define TFT_D4   18
#define TFT_D5   19
#define TFT_D6   21
#define TFT_D7   22

Arduino_DataBus *bus =
    new Arduino_ESP32PAR8(
      TFT_RS,
      TFT_CS,
      TFT_WR,
      TFT_RD,
      TFT_D0,
      TFT_D1,
      TFT_D2,
      TFT_D3,
      TFT_D4,
      TFT_D5,
      TFT_D6,
      TFT_D7
    );

// Initial driver selection: ILI9488.
// Change if your TFT uses another controller.
Arduino_GFX *gfx =
    new Arduino_ILI9488(
      bus,
      TFT_RST,
      1,
      false
    );

JPEGDEC jpeg;

// Maximum JPEG file size to buffer
const size_t MAX_JPEG_SIZE = 90000;

uint8_t *jpgBuffer = nullptr;

// Center JPEG on the landscape display
int imageX = 0;
int imageY = 0;

// JPEGDEC drawing callback
int drawJPEG(JPEGDRAW *draw) {
  gfx->draw16bitRGBBitmap(
    draw->x,
    draw->y,
    draw->pPixels,
    draw->iWidth,
    draw->iHeight
  );

  return 1;
}

bool downloadFrame(size_t &imageSize) {
  imageSize = 0;

  HTTPClient http;
  http.setTimeout(2500);

  if (!http.begin(cameraURL)) {
    Serial.println("HTTP begin failed");
    return false;
  }

  int code = http.GET();

  if (code != HTTP_CODE_OK) {
    Serial.printf("HTTP error: %d\n", code);
    http.end();
    return false;
  }

  int contentLength = http.getSize();

  if (contentLength <= 0 ||
      contentLength > (int)MAX_JPEG_SIZE) {
    Serial.printf(
      "Invalid frame size: %d\n",
      contentLength
    );

    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();

  size_t total = 0;
  unsigned long lastData = millis();

  while (total < (size_t)contentLength) {
    int availableBytes = stream->available();

    if (availableBytes > 0) {
      size_t remaining = contentLength - total;
      size_t count = min(
        remaining,
        (size_t)availableBytes
      );

      int n = stream->readBytes(
        jpgBuffer + total,
        count
      );

      if (n > 0) {
        total += n;
        lastData = millis();
      }
    } else {
      if (millis() - lastData > 2500) {
        break;
      }
      delay(1);
    }
  }

  http.end();

  if (total != (size_t)contentLength) {
    Serial.printf(
      "Incomplete JPEG: %u/%d\n",
      (unsigned int)total,
      contentLength
    );

    return false;
  }

  imageSize = total;
  return true;
}

bool displayFrame(size_t size) {
  if (!jpeg.openRAM(
        jpgBuffer,
        (int)size,
        drawJPEG
      )) {
    Serial.println("JPEG open failed");
    return false;
  }

  // Native RGB565 for Arduino_GFX
  jpeg.setPixelType(RGB565_LITTLE_ENDIAN);

  int width = jpeg.getWidth();
  int height = jpeg.getHeight();

  imageX = (gfx->width() - width) / 2;
  imageY = (gfx->height() - height) / 2;

  if (imageX < 0 || imageY < 0) {
    Serial.println("Image too large for TFT");
    jpeg.close();
    return false;
  }

  int result = jpeg.decode(
    imageX,
    imageY,
    0
  );

  jpeg.close();

  return result == 1;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32 Camera TFT Receiver");

  if (!gfx->begin()) {
    Serial.println("TFT initialization failed");
    while (true) delay(1000);
  }

  gfx->setRotation(1);
  gfx->fillScreen(0x0000);

  gfx->setTextColor(0xFFFF);
  gfx->setTextSize(2);
  gfx->setCursor(20, 20);
  gfx->println("ESP32 CAM VIEWER");

  // Allocate compressed JPEG frame buffer
  jpgBuffer = (uint8_t *)malloc(MAX_JPEG_SIZE);

  if (jpgBuffer == nullptr) {
    Serial.println("Not enough RAM for JPEG buffer");

    gfx->setCursor(20, 70);
    gfx->println("JPEG RAM ERROR");

    while (true) delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);

  gfx->setCursor(20, 70);
  gfx->println("Connecting WiFi...");

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < 20000) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");

    gfx->fillScreen(0x0000);
    gfx->setCursor(20, 20);
    gfx->println("Camera connected");
    delay(500);

    gfx->fillScreen(0x0000);
  } else {
    Serial.println("\nWiFi connection timeout");
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting WiFi...");
    WiFi.reconnect();
    delay(1000);
    return;
  }

  size_t frameSize = 0;
  unsigned long frameStart = millis();

  if (downloadFrame(frameSize)) {
    if (displayFrame(frameSize)) {
      unsigned long elapsed = millis() - frameStart;

      Serial.printf(
        "JPEG: %u bytes | Frame: %lu ms | FPS: %.1f\n",
        (unsigned int)frameSize,
        elapsed,
        elapsed > 0 ? 1000.0f / elapsed : 0.0f
      );
    } else {
      Serial.println("JPEG decode failed");
    }
  } else {
    Serial.println("Camera frame unavailable");
    delay(200);
  }
}
