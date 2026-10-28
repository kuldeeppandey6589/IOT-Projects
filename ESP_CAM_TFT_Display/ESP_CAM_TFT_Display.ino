#include <Arduino_GFX_Library.h>

// TFT control pins
#define TFT_RD   26
#define TFT_WR   25
#define TFT_RS   32
#define TFT_CS   33
#define TFT_RST  27

// Parallel data pins
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

// Start by testing the ILI9488 controller
Arduino_GFX *gfx = new Arduino_ILI9488(
  bus, TFT_RST, 1, false
);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Starting 3.5 TFT LCD...");

  if (!gfx->begin()) {
    Serial.println("Display initialization failed");
    while (true) delay(1000);
  }

  gfx->setRotation(1);

  Serial.println("Display initialization completed");
}

void loop() {
  gfx->fillScreen(0xF800);  // Red
  delay(1000);

  gfx->fillScreen(0x07E0);  // Green
  delay(1000);

  gfx->fillScreen(0x001F);  // Blue
  delay(1000);

  gfx->fillScreen(0xFFFF);  // White
  delay(1000);

  gfx->fillScreen(0x0000);  // Black

  gfx->setTextColor(0xFFFF);
  gfx->setTextSize(3);
  gfx->setCursor(40, 40);
  gfx->println("ESP32 TFT TEST");

  gfx->setTextColor(0x07E0);
  gfx->setTextSize(2);
  gfx->setCursor(40, 110);
  gfx->println("3.5 INCH LCD");

  gfx->setCursor(40, 160);
  gfx->println("DISPLAY WORKING");

  delay(3000);
}