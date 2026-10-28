
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// =============== WIFI SETTINGS ===============
const char* WIFI_SSID = "homeiot";
const char* WIFI_PASSWORD = "homeiot123";

// =============== RELAY PIN MAPPING ===========
// Assumed PCB mapping; verify before using.
const uint8_t relayPins[7] = {
  D0, D1, D2, D3, D5, D6, D7
};

const char* relayNames[7] = {
  "Light 1", "Light 2", "Light 3",
  "Fan", "Light 4", "Socket", "Light 5"
};

bool relayState[7] = {false, false, false, false,
                      false, false, false};

ESP8266WebServer server(80);

void applyRelay(uint8_t i) {
  // ULN2003 input HIGH turns its output transistor ON.
  // This energizes a correctly wired relay coil.
  digitalWrite(relayPins[i], relayState[i] ? HIGH : LOW);
}

String makePage() {
  String page;
  page.reserve(5000);

  page += F(
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Home Automation</title><style>"
    "body{font-family:Arial;background:#edf2f7;text-align:center;padding:16px}"
    "h2{color:#123}"
    ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(135px,1fr));"
    "gap:12px;max-width:700px;margin:auto}"
    ".card{background:white;padding:16px;border-radius:12px}"
    "button{border:0;border-radius:8px;padding:12px;color:white;width:100%;"
    "font-size:16px;cursor:pointer}"
    ".on{background:#16a34a}.off{background:#dc2626}"
    ".all{display:inline-block;width:auto;margin:5px;padding:12px 18px}"
    "</style></head><body><h2>ESP8266 Home Automation</h2>"
    "<p>7 Relay Control Panel</p>"
    "<a href='/all/on'><button class='all on'>ALL ON</button></a>"
    "<a href='/all/off'><button class='all off'>ALL OFF</button></a>"
    "<div class='grid'>"
  );

  for (uint8_t i = 0; i < 7; i++) {
    page += F("<div class='card'><h3>");
    page += relayNames[i];
    page += F("</h3><p>Status: <b>");
    page += relayState[i] ? F("ON") : F("OFF");
    page += F("</b></p><a href='/relay/");
    page += String(i + 1);
    page += relayState[i] ? F("/off'><button class='off'>TURN OFF")
                          : F("/on'><button class='on'>TURN ON");
    page += F("</button></a></div>");
  }

  page += F("</div></body></html>");
  return page;
}

void showPage() {
  server.send(200, "text/html", makePage());
}

void setAll(bool state) {
  for (uint8_t i = 0; i < 7; i++) {
    relayState[i] = state;
    applyRelay(i);
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize outputs to OFF.
  // Note: D3/GPIO0 is a boot-sensitive pin.
  for (uint8_t i = 0; i < 7; i++) {
    digitalWrite(relayPins[i], LOW);
    pinMode(relayPins[i], OUTPUT);
    relayState[i] = false;
    applyRelay(i);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Web address: http://");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, showPage);

  server.on("/all/on", HTTP_GET, []() {
    setAll(true);
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
  });

  server.on("/all/off", HTTP_GET, []() {
    setAll(false);
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
  });

  for (uint8_t i = 1; i <= 7; i++) {
    server.on(("/relay/" + String(i) + "/on").c_str(),
      HTTP_GET, [i]() {
        relayState[i - 1] = true;
        applyRelay(i - 1);
        server.sendHeader("Location", "/");
        server.send(303, "text/plain", "");
      });

    server.on(("/relay/" + String(i) + "/off").c_str(),
      HTTP_GET, [i]() {
        relayState[i - 1] = false;
        applyRelay(i - 1);
        server.sendHeader("Location", "/");
        server.send(303, "text/plain", "");
      });
  }

  server.begin();
  Serial.println("Home automation server started");
}

void loop() {
  server.handleClient();
}
