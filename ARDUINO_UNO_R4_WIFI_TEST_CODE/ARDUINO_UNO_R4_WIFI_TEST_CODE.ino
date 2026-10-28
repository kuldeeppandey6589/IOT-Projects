#include "WiFiS3.h"

// Define your network details
const char ssid[] = "homeiot";
const char pass[] = "homeiot123";

// Use Pin 7 to control the relay
const int RELAY_PIN = 7;

int status = WL_IDLE_STATUS;
WiFiServer server(80); // Start web server on port 80

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for Serial Monitor

  // Initialize relay pin as output and turn it off initially
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); 

  // Check for the presence of the WiFi module
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }

  // Attempt to connect to WiFi network
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(5000); // Wait 5 seconds for connection
  }

  // Start the server
  server.begin();
  
  // Print your local IP address to the Serial Monitor
  Serial.println("\nSuccessfully connected to WiFi!");
  Serial.print("Open a browser and navigate to: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Listen for incoming clients (web browsers)
  WiFiClient client = server.available();
  
  if (client) {
    Serial.println("New client connected.");
    String currentLine = ""; 
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        
        if (c == '\n') {
          // If the current line is blank, you got two newline characters in a row.
          // That's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // Display the HTML web page web interface
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<style>html { font-family: Arial; text-align: center; margin: 20px;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #f44336;}</style></head>");
            
            client.println("<body><h1>Arduino Uno R4 Relay Control</h1>");
            
            // Provide clickable dynamic control buttons
            client.println("<p><a href=\"/H\"><button class=\"button\">Turn Relay ON</button></a></p>");
            client.println("<p><a href=\"/L\"><button class=\"button button2\">Turn Relay OFF</button></a></p>");
            
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line:
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c; // Add character to the current line
        }

        // Check the client HTTP request to trigger the pin logic
        if (currentLine.endsWith("GET /H")) {
          digitalWrite(RELAY_PIN, HIGH); // Turn relay ON
          Serial.println("Relay Status: ON");
        }
        if (currentLine.endsWith("GET /L")) {
          digitalWrite(RELAY_PIN, LOW);  // Turn relay OFF
          Serial.println("Relay Status: OFF");
        }
      }
    }
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
  }
}
