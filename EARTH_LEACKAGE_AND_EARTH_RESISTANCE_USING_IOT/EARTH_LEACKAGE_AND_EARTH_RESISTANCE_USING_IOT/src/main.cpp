/* ============================================================
   EARTH RESISTANCE + SOIL LEAKAGE MONITOR
   CHANGE-BASED PUBLISHING + LOCAL RATE LIMITING TO AVOID
   ADAFRUIT IO THROTTLING
   ============================================================ */

#include <ESP8266WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

// ---------------- WiFi Credentials ----------------
#define WLAN_SSID "Robotutor"
#define WLAN_PASS "Robotutor"

// ---------------- Adafruit IO Settings -------------
#define AIO_SERVER "io.adafruit.com"
#define AIO_SERVERPORT 1883
#define AIO_USERNAME "earth251125"
#define AIO_KEY "aio_bPeM08gOJyJufEV3gIsWaTuzE3V4"

// MQTT CLIENT
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// MQTT FEEDS
Adafruit_MQTT_Publish feed_leak =
    Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/earth-leackage");

Adafruit_MQTT_Publish feed_res =
    Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/earth-resistance");

// ---------------- Hardware Pins --------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);

const float VCC = 3.3;
const float RKNOWN = 1000.0;
const float THRESH_RES_OHM = 50.0;

const uint8_t RELAY_PIN = D5;
const uint8_t RELAY_PIN_2 = D7;
const uint8_t SOIL_DO_PIN = D6;
const uint8_t RELAY_ACTIVE_LOW = 1;

// ---------- CHANGE-DETECTION VARIABLES ----------
float last_rprobe = -1;
bool last_soil_state = false;
const float R_CHANGE_THRESHOLD = 5.0;

// ---------- PUBLISH RATE LIMIT (LOCAL) ----------
const unsigned long MIN_PUBLISH_INTERVAL_MS = 5000UL; // minimum 5 seconds between publishes per feed
unsigned long last_publish_res_ms = 0;
unsigned long last_publish_leak_ms = 0;

// ---------------- FUNCTION DECLARATION --------------
void MQTT_connect();

// ---------------- RESISTANCE FUNCTIONS --------------
float readVadc(int samples = 30)
{
  uint32_t sum = 0;
  for (int i = 0; i < samples; i++)
  {
    sum += analogRead(A0);
    delay(4);
  }
  float avg = (float)sum / samples;
  return (avg / 1023.0) * VCC;
}

float computeResistance(float vprobe)
{
  if (vprobe <= 0.0005f)
    return INFINITY;
  if (vprobe >= VCC - 0.0005f)
    return 0.0f;
  return (vprobe * RKNOWN) / (VCC - vprobe);
}

// ---------------- SET RELAY -------------------------
void setRelay(bool on)
{
  if (RELAY_ACTIVE_LOW)
  {
    digitalWrite(RELAY_PIN, on ? HIGH : LOW);
    digitalWrite(RELAY_PIN_2, on ? HIGH : LOW);
  }
  else
  {
    digitalWrite(RELAY_PIN, on ? LOW : HIGH);
    digitalWrite(RELAY_PIN_2, on ? LOW : HIGH);
  }
}

// =====================================================
//                     SETUP
// =====================================================
void setup()
{
  Serial.begin(115200);
  delay(10);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(RELAY_PIN_2, OUTPUT);
  setRelay(false);

  pinMode(SOIL_DO_PIN, INPUT);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Earth & Soil Mon");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(400);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK");
}

// =====================================================
//                        LOOP
// =====================================================
void loop()
{
  MQTT_connect(); // stable like code-1

  float vprobe = readVadc();
  float rprobe = computeResistance(vprobe);

  bool soil_is_wet = (digitalRead(SOIL_DO_PIN) == LOW);
  bool badEarth = isfinite(rprobe) ? (rprobe > THRESH_RES_OHM) : true;

  bool alarm = badEarth || soil_is_wet;
  setRelay(alarm);

  // ---------------- LCD ----------------
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Earth R:");
  if (rprobe < 500.0f)
  {
    lcd.print("2 Ohm");
  }
  else if (rprobe >= 500.0f && rprobe < 2000.0f)
  {
    lcd.print("3 Ohm");
  }
  else if (rprobe >= 2000.0f && rprobe < 10000.0f)
  {
    digitalWrite(RELAY_PIN_2, LOW);
    lcd.print("5 Ohm");
  }
  else if (rprobe >= 10000.0f && rprobe < 100000.0f)
  {
    digitalWrite(RELAY_PIN_2, LOW);
    lcd.print("10 Ohm");
  }
  else 
  {
    digitalWrite(RELAY_PIN_2, LOW);
    lcd.print("20 Ohm");
  }
  // else
  // {
  //   digitalWrite(RELAY_PIN_2, LOW);
  //   lcd.print(">1M Ohm");
  // }

  lcd.setCursor(0, 1);
  if (soil_is_wet)
  {
    digitalWrite(RELAY_PIN, LOW);
    lcd.print("L : F & R :");
    lcd.print(vprobe*3);
  }
  else
  {
    digitalWrite(RELAY_PIN, HIGH);
    lcd.print("Leak! : NotFound ");
  }

  // ---------------- SERIAL ----------------
  Serial.print("Vprobe: ");
  Serial.print(vprobe, 3);
  Serial.print("  R: ");
  if (isfinite(rprobe))
    Serial.print(rprobe);
  else
    Serial.print("INF");

  Serial.print("  SoilWet: ");
  Serial.print(soil_is_wet);
  Serial.print("  Alarm: ");
  Serial.println(alarm);

  // =====================================================
  //             CHANGE-BASED MQTT PUBLISH + RATE LIMIT
  // =====================================================

  unsigned long now = millis();

  // ---------- resistance change detection ----------
  bool resistance_changed = false;
  if (isfinite(rprobe) && isfinite(last_rprobe))
  {
    if (abs(rprobe - last_rprobe) >= R_CHANGE_THRESHOLD)
      resistance_changed = true;
  }
  else
  {
    if (rprobe != last_rprobe)
      resistance_changed = true;
  }

  // If changed and minimum interval passed -> publish
  if (resistance_changed)
  {
    if (now - last_publish_res_ms >= MIN_PUBLISH_INTERVAL_MS)
    {
      char payload[32];
      if (isfinite(rprobe))
        snprintf(payload, sizeof(payload), "%.2f", rprobe);
      else
        snprintf(payload, sizeof(payload), "INF");

      if (feed_res.publish(payload))
      {
        Serial.println("MQTT: Resistance changed → published");
        last_rprobe = rprobe;
        last_publish_res_ms = now;
      }
      else
      {
        // publish failed (could be connection issue or server throttle)
        Serial.println("MQTT: Resistance publish FAILED (maybe throttled or disconnected)");
      }
    }
    else
    {
      Serial.println("MQTT: Resistance changed but publish suppressed (rate limit local).");
      // update last_rprobe so we don't keep detecting small changes repeatedly
      // but leave last_publish_res_ms alone so we still enforce the 5s gap
      last_rprobe = rprobe;
    }
  }

  // ---------- leak change detection ----------
  bool leak_changed = (soil_is_wet != last_soil_state);
  if (leak_changed)
  {
    if (now - last_publish_leak_ms >= MIN_PUBLISH_INTERVAL_MS)
    {
      if (feed_leak.publish(soil_is_wet ? 1 : 0))
      {
        Serial.println("MQTT: Soil leak changed → published");
        last_soil_state = soil_is_wet;
        last_publish_leak_ms = now;
      }
      else
      {
        Serial.println("MQTT: Soil leak publish FAILED (maybe throttled or disconnected)");
      }
    }
    else
    {
      Serial.println("MQTT: Soil leak changed but publish suppressed (rate limit local).");
      // update last_soil_state to avoid repeated detections until next publish window
      last_soil_state = soil_is_wet;
    }
  }

  // small delay to stabilise loop (keep reasonably low but avoid busy-wait)
  delay(800);
}

// =====================================================
//             SIMPLE MQTT CONNECT w/ BACKOFF
// =====================================================
void MQTT_connect()
{
  static unsigned long backoff_ms = 5000UL;    // initial backoff 5s
  static const unsigned long MAX_BACKOFF_MS = 60000UL; // cap backoff to 60s
  int8_t ret;

  if (mqtt.connected())
  {
    // pump the client to keep connection alive
    mqtt.processPackets(10);
    return;
  }

  Serial.print("Connecting to MQTT... ");

  while ((ret = mqtt.connect()) != 0)
  {
    Serial.print("Failed: ");
    Serial.print(mqtt.connectErrorString(ret));
    Serial.print("  Backing off ");
    Serial.print(backoff_ms / 1000);
    Serial.println("s");

    mqtt.disconnect();
    delay(backoff_ms);

    // exponential backoff with cap
    backoff_ms = backoff_ms * 2;
    if (backoff_ms > MAX_BACKOFF_MS)
      backoff_ms = MAX_BACKOFF_MS;
  }

  // success: reset backoff
  backoff_ms = 5000UL;
  Serial.println("MQTT Connected!");
}
