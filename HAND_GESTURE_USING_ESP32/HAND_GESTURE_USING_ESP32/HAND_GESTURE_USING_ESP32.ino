#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "homeiot";
const char* password = "homeiot123";

WebServer server(80);

// Relay pins
#define RELAY1 26
#define RELAY2 27

void setup() {
  Serial.begin(115200);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);

  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);

  WiFi.begin(ssid, password);

  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Connected");
  Serial.println(WiFi.localIP());

  server.on("/lighton", []() {
    digitalWrite(RELAY1, LOW);
    server.send(200, "text/plain", "Light ON");
  });

  server.on("/lightoff", []() {
    digitalWrite(RELAY1, HIGH);
    server.send(200, "text/plain", "Light OFF");
  });

  server.on("/fanon", []() {
    digitalWrite(RELAY2, LOW);
    server.send(200, "text/plain", "Fan ON");
  });

  server.on("/fanoff", []() {
    digitalWrite(RELAY2, HIGH);
    server.send(200, "text/plain", "Fan OFF");
  });

  server.on("/alloff", []() {
    digitalWrite(RELAY1, HIGH);
    digitalWrite(RELAY2, HIGH);
    server.send(200, "text/plain", "ALL OFF");
  });

  server.begin();
}

void loop() {
  server.handleClient();
}