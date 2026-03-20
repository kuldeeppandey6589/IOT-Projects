#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <U8g2lib.h>
#include <string.h>

// ================== CONFIG ==================
// Device 1: MY_ID = 1, PEER_ID = 2
// Device 2: MY_ID = 2, PEER_ID = 1
#define MY_ID   1
#define PEER_ID 2

// ================== DISPLAY SETUP (U8g2) ==================D
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,
  U8X8_PIN_NONE
);

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

// ================== PINS (ESP32) ==================
// Encoder
const int ENC_A_PIN  = 34;   // CLK
const int ENC_B_PIN  = 35;   // DT
const int ENC_SW_PIN = 32;   // Encoder push

// Buttons
const int BACKSPACE_PIN = 27;  // Backspace
const int SEND_PIN      = 26;  // Send

// Buzzer
const int BUZZER_PIN = 25;   // choose any free GPIO (25 is safe)

// LoRa (RA-02)
#define LORA_NSS   5
#define LORA_RST   14
#define LORA_DIO0  2

#define LORA_FREQ_HZ  433E6
#define LORA_SYNCWORD 0xF3

// ================== ENCODER STATE ==================
volatile long encoderTicks = 0;
volatile int  lastEncoded  = 0;
long encoderStepAccum      = 0;

// ================== UI MODES ==================
enum UiMode { MODE_TYPING, MODE_HISTORY };
UiMode uiMode = MODE_TYPING;

// Quick-reply sub-mode
bool quickMenuActive = false;
int  quickIndex      = 0;
const char* quickMessages[] = {"yes", "no", "roger", "help", "busy"};
const int   QUICK_MSG_COUNT = 5;

// ================== BUTTON STATE ==================
bool lastEncButtonState      = HIGH;
unsigned long encButtonPressTime = 0;
const unsigned long ENC_SHORT_PRESS_MIN = 80;
const unsigned long ENC_LONG_PRESS_MIN  = 600;

bool lastBackspaceState      = HIGH;
unsigned long backspacePressTime = 0;
const unsigned long BACKSPACE_SHORT_PRESS_MIN = 60;
const unsigned long BACKSPACE_LONG_PRESS_MIN  = 600;

bool lastSendState           = HIGH;
unsigned long sendPressTime  = 0;
const unsigned long SEND_SHORT_PRESS_MIN = 60;
const unsigned long SEND_LONG_PRESS_MIN  = 800;

// ================== CHARACTER SETS ==================
// 0 = UPPER, 1 = lower, 2 = numbers, 3 = punctuation
int modeIndex = 0;

const char lettersUpper[]   = "ABCDE FGHIJ KLMNO PQRST UVWXY Z";
const char lettersLower[]   = "abcde fghij klmno pqrst uvwxy z";
const char numbersCharset[] = "0123456789";
const char punctCharset[]   = ".,!?'-:;()[]{}@#/\\\" ";

const char* currentCharset = lettersUpper;
int currentCharsetLen      = 0;
int currentIndex           = 0;

// ================== TYPING / TEXT BUFFER ==================
unsigned long lastMoveTime = 0;
bool charCommitted          = true;
const unsigned long COMMIT_DELAY = 700; // ms pause to commit

#define MAX_TEXT_LEN 80
char textBuffer[MAX_TEXT_LEN + 1];
int  textLength = 0;

// ================== UI LAYOUT ==================
const int TEXT_AREA_X = 0;
const int TEXT_AREA_Y = 10;
const int TEXT_AREA_W = SCREEN_WIDTH;
const int TEXT_AREA_H = 36;

const int CHAR_W = 6;
const int CHAR_H = 10;

// ================== CURSOR BLINK ==================
bool cursorVisible            = true;
unsigned long lastCursorBlink = 0;
const unsigned long CURSOR_BLINK_INTERVAL = 400;

// ================== CHAT HISTORY ==================
enum MsgStatus : uint8_t {
  MSG_PENDING,
  MSG_DELIVERED,
  MSG_FAILED,
  MSG_RECV
};

struct ChatMessage {
  bool     fromMe;
  uint8_t  seq;
  MsgStatus status;
  char     text[MAX_TEXT_LEN + 1];   // same size as typing buffer
};


const int HISTORY_SIZE = 12;
ChatMessage historyBuf[HISTORY_SIZE];
int historyStart = 0;
int historySize  = 0;

int  historyScrollOffset   = 0;
bool historyPinnedToNewest = true; // auto-scroll when true

// ================== LORA / PROTOCOL STATE ==================
uint8_t msgSeq = 0;

bool waitingAck     = false;
uint8_t waitingSeq  = 0;
unsigned long lastSendMs = 0;
uint8_t retryCount       = 0;

const unsigned long ACK_TIMEOUT_MS = 1500;
const uint8_t MAX_RETRIES          = 3;

// Link + unread state
bool    linkEstablished = false;
uint8_t unreadCount     = 0;

// Heartbeat for link detection
const unsigned long HEARTBEAT_PERIOD_MS = 5000;
unsigned long lastHeartbeatMs = 0;
unsigned long lastLinkRxMs    = 0;
const unsigned long LINK_TIMEOUT_MS = 15000;

// Screensaver / auto-dim
unsigned long lastActivityMs = 0;
bool displaySleeping         = false;
const unsigned long SCREENSAVER_TIMEOUT_MS = 60000;

// ================== FN DECLS ==================
void updateCharset();
void updateDisplay();
void updateDisplayTyping();
void updateDisplayHistory();
void updateDisplayQuickMenu();
void handleEncoderTyping(long diff);
void handleEncoderHistory(long diff);
void handleEncoderQuick(long diff);
void handleEncButton();
void handleBackspaceButton();
void handleSendButton();
void handleSelection();
void handleCursorBlink();
void handleLoRa();
void handleAckTimeout();
void sendChatMessage(const char* txt);
void sendLoRaPacket(const String& pkt);
void storeOutgoingMessage(uint8_t seq, const char* txt);
void storeIncomingMessage(uint8_t seq, const String& txt);
int  findOutgoingBySeq(uint8_t seq);
bool incomingAlreadyExists(uint8_t seq);
void setMessageStatus(uint8_t seq, MsgStatus st);
void addToHistory(const ChatMessage& msg);
void drawStatusIcons(const char* leftLabel);
void noteActivity();
void checkScreensaver();
void handleHeartbeat();
void handleLinkTimeout();

// ================== ENCODER ISR ==================
void IRAM_ATTR encoderISR() {
  int MSB = digitalRead(ENC_A_PIN);
  int LSB = digitalRead(ENC_B_PIN);
  int encoded = (MSB << 1) | LSB;
  int sum = (lastEncoded << 2) | encoded;

  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderTicks++;
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderTicks--;

  lastEncoded = encoded;
}

void beepBeep1s() {
  // Total ~1 second: 200ms ON, 200ms OFF, 200ms ON, 400ms OFF
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);
  delay(200);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);
  delay(400);
}


// ================== SETUP ==================
void setup() {
  pinMode(ENC_A_PIN,  INPUT_PULLUP);
  pinMode(ENC_B_PIN,  INPUT_PULLUP);
  pinMode(ENC_SW_PIN, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(BACKSPACE_PIN, INPUT_PULLUP);
  pinMode(SEND_PIN,      INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENC_A_PIN), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B_PIN), encoderISR, CHANGE);

  Serial.begin(115200);

  // I2C
  Wire.begin(21, 22);

  // OLED
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 12, "ESP32 LoRaChat");
  u8g2.sendBuffer();
  delay(500);

  textBuffer[0] = '\0';

  uiMode = MODE_TYPING;
  updateCharset();

  lastMoveTime     = millis();
  charCommitted    = true;
  lastCursorBlink  = millis();
  cursorVisible    = true;

  // LoRa
  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, "LoRa init...");
  u8g2.sendBuffer();

  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ_HZ)) {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, "LoRa FAIL");
    u8g2.sendBuffer();
    Serial.println("LoRa init FAIL");
    while (true) {}
  }

  LoRa.setSyncWord(LORA_SYNCWORD);
  LoRa.setTxPower(17);

  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, "LoRa OK");
  u8g2.sendBuffer();
  Serial.println("LoRa init OK");
  delay(500);

  lastActivityMs = millis();
  updateDisplay();
}

// ================== MAIN LOOP ==================
void loop() {
  static long lastTicks = 0;
  long ticks;

  noInterrupts();
  ticks = encoderTicks;
  interrupts();

  long diff = ticks - lastTicks;
  lastTicks = ticks;

  if (diff != 0) {
    noteActivity();
    if (quickMenuActive) {
      handleEncoderQuick(diff);
    } else if (uiMode == MODE_TYPING) {
      handleEncoderTyping(diff);
    } else {
      handleEncoderHistory(diff);
    }
  }

  handleEncButton();
  handleBackspaceButton();
  handleSendButton();

  if (uiMode == MODE_TYPING && !quickMenuActive) {
    handleSelection();
    handleCursorBlink();
  }

  handleLoRa();
  handleAckTimeout();
  handleHeartbeat();
  handleLinkTimeout();
  checkScreensaver();
}

// ================== ACTIVITY / SCREENSAVER ==================
void noteActivity() {
  lastActivityMs = millis();
  if (displaySleeping) {
    displaySleeping = false;
    u8g2.setPowerSave(0);
    updateDisplay();
  }
}

void checkScreensaver() {
  unsigned long now = millis();
  if (!displaySleeping && (now - lastActivityMs >= SCREENSAVER_TIMEOUT_MS)) {
    displaySleeping = true;
    u8g2.setPowerSave(1);
  }
}

// ================== CHARSET UPDATE ==================
void updateCharset() {
  if (uiMode == MODE_TYPING) {
    switch (modeIndex) {
      case 0: currentCharset = lettersUpper;   break;
      case 1: currentCharset = lettersLower;   break;
      case 2: currentCharset = numbersCharset; break;
      case 3: currentCharset = punctCharset;   break;
    }
  }

  currentCharsetLen = strlen(currentCharset);
  if (currentIndex >= currentCharsetLen) currentIndex = currentCharsetLen - 1;
  if (currentIndex < 0)                  currentIndex = 0;

  charCommitted     = true;
  lastMoveTime      = millis();
  encoderStepAccum  = 0;

  updateDisplay();
}

// ================== ENCODER HANDLERS ==================
void handleEncoderTyping(long diff) {
  encoderStepAccum += diff;

  int oldIndex = currentIndex;

  while (encoderStepAccum >= 3) {
    currentIndex++;
    encoderStepAccum -= 3;
  }
  while (encoderStepAccum <= -3) {
    currentIndex--;
    encoderStepAccum += 3;
  }

  if (currentIndex < 0) currentIndex = currentCharsetLen - 1;
  if (currentIndex >= currentCharsetLen) currentIndex = 0;

  if (currentIndex != oldIndex) {
    lastMoveTime  = millis();
    charCommitted = false;
    updateDisplay();
  }
}

void handleEncoderHistory(long diff) {
  encoderStepAccum += diff;

  int steps = 0;
  while (encoderStepAccum >= 3) {
    steps++;
    encoderStepAccum -= 3;
  }
  while (encoderStepAccum <= -3) {
    steps--;
    encoderStepAccum += 3;
  }

  if (steps != 0 && historySize > 0) {
    historyScrollOffset += steps;
    int maxOffset = (historySize - 1);
    if (historyScrollOffset < 0) historyScrollOffset = 0;
    if (historyScrollOffset > maxOffset) historyScrollOffset = maxOffset;

    historyPinnedToNewest = (historyScrollOffset == 0);
    updateDisplay();
  }
}

void handleEncoderQuick(long diff) {
  if (diff > 0) {
    quickIndex = (quickIndex + 1) % QUICK_MSG_COUNT;
  } else if (diff < 0) {
    quickIndex = (quickIndex - 1 + QUICK_MSG_COUNT) % QUICK_MSG_COUNT;
  }
  updateDisplay();
}

// ================== ENCODER BUTTON ==================
void handleEncButton() {
  bool s = digitalRead(ENC_SW_PIN);
  unsigned long now = millis();

  if (lastEncButtonState == HIGH && s == LOW) {
    encButtonPressTime = now;
  }

  if (lastEncButtonState == LOW && s == HIGH) {
    unsigned long dur = now - encButtonPressTime;

    // If we are in quick menu, encoder button is used for confirm/cancel
    if (quickMenuActive) {
      noteActivity();
      if (dur >= ENC_LONG_PRESS_MIN) {
        quickMenuActive = false;
        updateDisplay();
      } else if (dur >= ENC_SHORT_PRESS_MIN) {
        if (!waitingAck) {
          sendChatMessage(quickMessages[quickIndex]);
        }
        quickMenuActive = false;
        updateDisplay();
      }
      lastEncButtonState = s;
      return;
    }

    if (dur >= ENC_LONG_PRESS_MIN) {
      // Long press: toggle Typing <-> History
      noteActivity();
      if (uiMode == MODE_TYPING) {
        uiMode = MODE_HISTORY;
        historyScrollOffset   = 0;
        historyPinnedToNewest = true;
        unreadCount           = 0;  // you've seen messages
      } else {
        uiMode = MODE_TYPING;
      }
      updateDisplay();
    } else if (dur >= ENC_SHORT_PRESS_MIN) {
      noteActivity();
      if (uiMode == MODE_TYPING) {
        modeIndex = (modeIndex + 1) % 4;
        updateCharset();
      } else {
        historyScrollOffset   = 0;
        historyPinnedToNewest = true;
        updateDisplay();
      }
    }
  }

  lastEncButtonState = s;
}

// ================== BACKSPACE BUTTON ==================
void handleBackspaceButton() {
  bool s = digitalRead(BACKSPACE_PIN);
  unsigned long now = millis();

  if (lastBackspaceState == HIGH && s == LOW) {
    backspacePressTime = now;
  }

  if (lastBackspaceState == LOW && s == HIGH) {
    unsigned long dur = now - backspacePressTime;

    // In quick menu: backspace = cancel
    if (quickMenuActive) {
      if (dur >= BACKSPACE_SHORT_PRESS_MIN) {
        noteActivity();
        quickMenuActive = false;
        updateDisplay();
      }
      lastBackspaceState = s;
      return;
    }

    if (dur >= BACKSPACE_LONG_PRESS_MIN) {
      noteActivity();
      if (uiMode == MODE_TYPING) {
        textLength    = 0;
        textBuffer[0] = '\0';
        updateDisplay();
      } else {
        uiMode = MODE_TYPING;
        updateDisplay();
      }
    } else if (dur >= BACKSPACE_SHORT_PRESS_MIN) {
      noteActivity();
      if (uiMode == MODE_TYPING) {
        if (textLength > 0) {
          textLength--;
          textBuffer[textLength] = '\0';
          updateDisplay();
        }
      } else {
        uiMode = MODE_TYPING;
        updateDisplay();
      }
    }
  }

  lastBackspaceState = s;
}

// ================== SEND BUTTON ==================
void handleSendButton() {
  bool s = digitalRead(SEND_PIN);
  unsigned long now = millis();

  if (lastSendState == HIGH && s == LOW) {
    sendPressTime = now;
  }

  if (lastSendState == LOW && s == HIGH) {
    unsigned long dur = now - sendPressTime;

    // Inside quick menu: short press acts as "send selected"
    if (quickMenuActive) {
      if (dur >= SEND_SHORT_PRESS_MIN) {
        noteActivity();
        if (!waitingAck) {
          sendChatMessage(quickMessages[quickIndex]);
        }
        quickMenuActive = false;
        updateDisplay();
      }
      lastSendState = s;
      return;
    }

    if (dur >= SEND_LONG_PRESS_MIN) {
      // Enter quick-reply menu (typing mode only)
      if (uiMode == MODE_TYPING) {
        noteActivity();
        quickMenuActive = true;
        quickIndex      = 0;
        updateDisplay();
      }
    } else if (dur >= SEND_SHORT_PRESS_MIN) {
      noteActivity();
      if (uiMode == MODE_TYPING && !quickMenuActive) {
        if (textLength > 0 && !waitingAck) {
          sendChatMessage(textBuffer);
          textLength    = 0;
          textBuffer[0] = '\0';
          updateDisplay();
        }
      } else if (uiMode == MODE_HISTORY) {
        uiMode = MODE_TYPING;
        updateDisplay();
      }
    }
  }

  lastSendState = s;
}

// ================== CHARACTER COMMIT ==================
void handleSelection() {
  unsigned long now = millis();

  if (!charCommitted && (now - lastMoveTime) >= COMMIT_DELAY) {
    if (textLength < MAX_TEXT_LEN) {
      char c = currentCharset[currentIndex];
      textBuffer[textLength++] = c;
      textBuffer[textLength]   = '\0';
      Serial.print(c);
      charCommitted = true;
      updateDisplay();
    }
  }
}

// ================== CURSOR BLINK ==================
void handleCursorBlink() {
  unsigned long now = millis();
  if (now - lastCursorBlink >= CURSOR_BLINK_INTERVAL) {
    lastCursorBlink = now;
    cursorVisible   = !cursorVisible;
    updateDisplay();
  }
}

// ================== LORA RX HANDLING ==================
void handleLoRa() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String line;
  while (LoRa.available()) {
    char c = (char)LoRa.read();
    line += c;
  }
  line.trim();
  if (line.length() == 0) return;

  Serial.print("\nLoRa RX: ");
  Serial.println(line);

  int p0 = line.indexOf('|');
  if (p0 < 0) return;
  char type = line.charAt(0);

  int p1 = line.indexOf('|', p0 + 1);
  int p2 = line.indexOf('|', p1 + 1);
  int p3 = line.indexOf('|', p2 + 1);

  if (p1 < 0 || p2 < 0 || p3 < 0) return;

  int src = line.substring(p0 + 1, p1).toInt();
  int dst = line.substring(p1 + 1, p2).toInt();
  int seq = line.substring(p2 + 1, p3).toInt();

  if (dst != MY_ID) return;

  // Any packet from our peer counts as link alive
  if (src == PEER_ID) {
    linkEstablished = true;
    lastLinkRxMs    = millis();
    updateDisplay();
  }

  if (type == 'H') {
    // heartbeat, nothing else to do
    return;
  } else if (type == 'A') {
    if (waitingAck && seq == waitingSeq && src == PEER_ID) {
      waitingAck = false;
      setMessageStatus(seq, MSG_DELIVERED);
      updateDisplay();
    }
  } else if (type == 'C') {
    if (src != PEER_ID) return;

    String txt = line.substring(p3 + 1);

if (!incomingAlreadyExists((uint8_t)seq)) {
  storeIncomingMessage((uint8_t)seq, txt);
  unreadCount++;

  beepBeep1s();     // ✅ buzzer alert on receive

  noteActivity();   // wake screen & reset idle
  updateDisplay();
}


    String ack = String("A|") + MY_ID + "|" + src + "|" + seq + "|";
    sendLoRaPacket(ack);
  }
}

// ================== HEARTBEAT / LINK TIMEOUT ==================
void handleHeartbeat() {
  unsigned long now = millis();
  if (now - lastHeartbeatMs >= HEARTBEAT_PERIOD_MS) {
    lastHeartbeatMs = now;
    // Simple heartbeat packet, seq 0
    String hb = String("H|") + MY_ID + "|" + PEER_ID + "|0|";
    sendLoRaPacket(hb);
  }
}

void handleLinkTimeout() {
  if (!linkEstablished) return;
  unsigned long now = millis();
  if (now - lastLinkRxMs > LINK_TIMEOUT_MS) {
    linkEstablished = false;
    updateDisplay();
  }
}

// ================== ACK TIMEOUT / RETRY ==================
void handleAckTimeout() {
  if (!waitingAck) return;

  unsigned long now = millis();
  if (now - lastSendMs >= ACK_TIMEOUT_MS) {
    if (retryCount < MAX_RETRIES) {
      Serial.println("ACK timeout, retrying...");
      int idx = findOutgoingBySeq(waitingSeq);
      if (idx >= 0) {
        ChatMessage &m = historyBuf[idx];
        String pkt = String("C|") + MY_ID + "|" + PEER_ID + "|" + waitingSeq + "|" + m.text;
        sendLoRaPacket(pkt);
        retryCount++;
        lastSendMs = now;
      } else {
        waitingAck = false;
      }
    } else {
      waitingAck = false;
      setMessageStatus(waitingSeq, MSG_FAILED);
      Serial.println("ACK failed after retries");
      updateDisplay();
    }
  }
}

// ================== SEND CHAT MESSAGE ==================
void sendChatMessage(const char* txt) {
  msgSeq++;
  uint8_t seq = msgSeq;

  storeOutgoingMessage(seq, txt);

  String pkt = String("C|") + MY_ID + "|" + PEER_ID + "|" + seq + "|" + txt;
  sendLoRaPacket(pkt);

  waitingAck = true;
  waitingSeq = seq;
  retryCount = 0;
  lastSendMs = millis();

  noteActivity();
  Serial.print("LoRa TX: ");
  Serial.println(pkt);
}

// ================== LOW-LEVEL LORA SEND ==================
void sendLoRaPacket(const String& pkt) {
  LoRa.beginPacket();
  LoRa.print(pkt);
  LoRa.endPacket();
}

// ================== HISTORY HELPERS ==================
void addToHistory(const ChatMessage& msg) {
  int insertPos = (historyStart + historySize) % HISTORY_SIZE;
  historyBuf[insertPos] = msg;

  if (historySize < HISTORY_SIZE) {
    historySize++;
  } else {
    historyStart = (historyStart + 1) % HISTORY_SIZE;
  }

  if (historyPinnedToNewest) {
    historyScrollOffset = 0;
  }
}

void storeOutgoingMessage(uint8_t seq, const char* txt) {
  ChatMessage msg;
  msg.fromMe = true;
  msg.seq    = seq;
  msg.status = MSG_PENDING;
  strncpy(msg.text, txt, sizeof(msg.text) - 1);
  msg.text[sizeof(msg.text) - 1] = '\0';

  addToHistory(msg);
}

void storeIncomingMessage(uint8_t seq, const String& txt) {
  ChatMessage msg;
  msg.fromMe = false;
  msg.seq    = seq;
  msg.status = MSG_RECV;
  txt.toCharArray(msg.text, sizeof(msg.text));

  addToHistory(msg);
}

int findOutgoingBySeq(uint8_t seq) {
  for (int i = 0; i < historySize; i++) {
    int idx = (historyStart + i) % HISTORY_SIZE;
    ChatMessage &m = historyBuf[idx];
    if (m.fromMe && m.seq == seq) return idx;
  }
  return -1;
}

bool incomingAlreadyExists(uint8_t seq) {
  for (int i = 0; i < historySize; i++) {
    int idx = (historyStart + i) % HISTORY_SIZE;
    ChatMessage &m = historyBuf[idx];
    if (!m.fromMe && m.seq == seq) return true;
  }
  return false;
}

void setMessageStatus(uint8_t seq, MsgStatus st) {
  int idx = findOutgoingBySeq(seq);
  if (idx >= 0) {
    historyBuf[idx].status = st;
  }
}

// ================== DISPLAY ROUTER ==================
void updateDisplay() {
  if (quickMenuActive) {
    updateDisplayQuickMenu();
  } else if (uiMode == MODE_TYPING) {
    updateDisplayTyping();
  } else {
    updateDisplayHistory();
  }
}

// ---- TYPING MODE DISPLAY ----
void updateDisplayTyping() {
  u8g2.clearBuffer();

  const char* modeStr =
    (modeIndex == 0) ? "ABC" :
    (modeIndex == 1) ? "abc" :
    (modeIndex == 2) ? "123" : "PUNC";

  drawStatusIcons(modeStr);

  u8g2.drawFrame(TEXT_AREA_X, TEXT_AREA_Y, TEXT_AREA_W, TEXT_AREA_H);

  int innerX = TEXT_AREA_X + 2;
  int innerY = TEXT_AREA_Y + 2;
  int innerW = TEXT_AREA_W - 4;
  int innerH = TEXT_AREA_H - 4;

  int charsPerLine = innerW / CHAR_W;
  if (charsPerLine < 1) charsPerLine = 1;
  int lines = innerH / CHAR_H;
  if (lines < 1) lines = 1;

  int maxVisible = charsPerLine * lines;
  int startIndex = (textLength > maxVisible) ? textLength - maxVisible : 0;

  int x = innerX;
  int y = innerY + CHAR_H;

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(x, y);

  for (int i = startIndex; i < textLength; i++) {
    char c = textBuffer[i];
    u8g2.print(c);
    x += CHAR_W;

    if (x > (innerX + innerW - CHAR_W)) {
      x = innerX;
      y += CHAR_H;
      u8g2.setCursor(x, y);
    }
  }

  if (cursorVisible && (y - CHAR_H + 2) <= (innerY + innerH - CHAR_H)) {
    int cx = x;
    int cyTop = y - CHAR_H + 2;
    int cyBot = cyTop + CHAR_H - 2;
    u8g2.drawLine(cx, cyTop, cx, cyBot);
  }

  char c = currentCharset[currentIndex];
  if (c == ' ') {
    const char* label = "SPACE";
    int labelWidth = strlen(label) * CHAR_W;
    int labelX     = (SCREEN_WIDTH - labelWidth) / 2;
    int labelY     = SCREEN_HEIGHT - 2;
    u8g2.setCursor(labelX, labelY);
    u8g2.print(label);
  } else {
    u8g2.setFont(u8g2_font_fub11_tf);
    char ch[2] = { c, '\0' };

    uint16_t w = u8g2.getUTF8Width(ch);
    int charX = (SCREEN_WIDTH - w) / 2;
    int charY = SCREEN_HEIGHT - 4;
    u8g2.setCursor(charX, charY);
    u8g2.print(ch);
  }

  u8g2.sendBuffer();
}

// ---- HISTORY MODE DISPLAY (with bubbles + status icons) ----
void updateDisplayHistory() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  drawStatusIcons("HISTORY");

  const int boxY = 12;
  const int boxH = SCREEN_HEIGHT - boxY - 2;
  u8g2.drawFrame(0, boxY, SCREEN_WIDTH, boxH);

  int innerX = 2;
  int innerY = boxY + 2;
  int innerW = SCREEN_WIDTH - 4;
  int innerH = boxH - 4;

  int charsPerLine = innerW / CHAR_W;
  if (charsPerLine < 1) charsPerLine = 1;
  int lines = innerH / CHAR_H;
  if (lines < 1) lines = 1;

  int newestOffset = historySize - 1;

  for (int row = 0; row < lines; row++) {
    int msgOffset = newestOffset - (historyScrollOffset + row);
    if (msgOffset < 0 || msgOffset >= historySize) continue;

    int idx = (historyStart + msgOffset) % HISTORY_SIZE;
    ChatMessage &m = historyBuf[idx];

    const char* prefix = m.fromMe ? "I:" : "HE:";
    int prefixChars    = strlen(prefix) + 1; // "I:" + space

    int maxCharsForMsg = charsPerLine - prefixChars;
    if (maxCharsForMsg < 0) maxCharsForMsg = 0;

    // Compute actual displayed length (truncate text)
    int msgLenDisplay = 0;
    while (msgLenDisplay < maxCharsForMsg && m.text[msgLenDisplay] != '\0') {
      msgLenDisplay++;
    }

       int bubbleChars = prefixChars + msgLenDisplay;
    int bubbleW     = bubbleChars * CHAR_W + 4;  // some padding

    int bubbleX;
    if (m.fromMe) {
      // I: on LEFT
      bubbleX = innerX;
    } else {
      // HE: on RIGHT
      bubbleX = innerX + innerW - bubbleW;
    }


    int bubbleY = innerY + row * CHAR_H;
    int bubbleH = CHAR_H;

    // Chat bubble (simple rectangle)
    u8g2.drawRFrame(bubbleX, bubbleY, bubbleW, bubbleH, 2); // rounded corners

    int textX = bubbleX + 2;
    int textY = bubbleY + CHAR_H - 2;
    u8g2.setCursor(textX, textY);

    // Print prefix and space
    u8g2.print(prefix);
    u8g2.print(' ');

    // Print text
    for (int i = 0; i < msgLenDisplay; i++) {
      u8g2.print(m.text[i]);
    }

    // Status icon on right side of bubble only for my messages
    // Status icon on far right side of text area only for my messages
    if (m.fromMe) {
      int iconX = innerX + innerW - 6;         // fixed near right edge of box
      int iconY = bubbleY + bubbleH / 2;       // vertically centered to the bubble

      switch (m.status) {
        case MSG_PENDING:
          // small dot •
          u8g2.drawDisc(iconX, iconY, 1, U8G2_DRAW_ALL);
          break;

        case MSG_DELIVERED:
          // small tick ✓
          u8g2.drawLine(iconX - 2, iconY + 2, iconX,     iconY + 4);
          u8g2.drawLine(iconX,     iconY + 4, iconX + 4, iconY - 2);
          break;

        case MSG_FAILED:
          // small cross ✕
          u8g2.drawLine(iconX - 2, iconY - 2, iconX + 2, iconY + 2);
          u8g2.drawLine(iconX - 2, iconY + 2, iconX + 2, iconY - 2);
          break;

        default:
          break;
      }
    }

  }

  u8g2.sendBuffer();
}

// ---- QUICK-REPLY MENU DISPLAY ----
void updateDisplayQuickMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  drawStatusIcons("QUICK");

  const int boxY = 12;
  const int boxH = SCREEN_HEIGHT - boxY - 2;
  u8g2.drawFrame(0, boxY, SCREEN_WIDTH, boxH);

  int centerY = boxY + boxH / 2;

  // Show current quick message big in center
  u8g2.setFont(u8g2_font_fub11_tf);
  const char* msg = quickMessages[quickIndex];
  int w = u8g2.getUTF8Width(msg);
  int x = (SCREEN_WIDTH - w) / 2;
  int y = centerY + 4;
  u8g2.setCursor(x, y);
  u8g2.print(msg);

  // Small hint
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(6, boxY + boxH - 4);
  u8g2.print("snd btn:SND bksp:CNCL");

  u8g2.sendBuffer();
}

// ---- TOP BAR: label + unread + link ----
void drawStatusIcons(const char* leftLabel) {
  u8g2.setFont(u8g2_font_6x10_tf);

  // Left label
  u8g2.setCursor(0, 10);
  u8g2.print(leftLabel);

  // Center: unread count
  if (unreadCount > 0) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%u", unreadCount);
    int w = u8g2.getUTF8Width(buf);
    int x = (SCREEN_WIDTH - w) / 2;
    u8g2.setCursor(x, 10);
    u8g2.print(buf);
  }

  // Right: link indicator
  if (linkEstablished) {
    const char* linkStr = "<->";
    int w = u8g2.getUTF8Width(linkStr);
    int x = SCREEN_WIDTH - w;
    u8g2.setCursor(x, 10);
    u8g2.print(linkStr);
  }
}
