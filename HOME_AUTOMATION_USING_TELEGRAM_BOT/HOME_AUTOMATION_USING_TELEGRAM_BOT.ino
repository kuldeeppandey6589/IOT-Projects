#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>


// WiFi credentials
const char* ssid = "homeiot";
const char* password = "homeiot123";

// Telegram BOT Token
#define BOTtoken "8791037401:AAHKW7yHbrAnMtFpcfA89spgr3SH-GvN5Ow"

// Chat ID
#define CHAT_ID "1557169121"

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Relay Pins
int relay1 = 5;
int relay2 = 18;
int relay3 = 19;
int relay4 = 21;

void setup() {
  Serial.begin(115200);

  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  pinMode(relay3, OUTPUT);
  pinMode(relay4, OUTPUT);

  // Initially OFF (Relay active LOW)
  digitalWrite(relay1, HIGH);
  digitalWrite(relay2, HIGH);
  digitalWrite(relay3, HIGH);
  digitalWrite(relay4, HIGH);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");

  client.setInsecure(); // Required for HTTPS

  bot.sendMessage(CHAT_ID, "🤖 Home Automation Bot Started", "");
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {

    String text = bot.messages[i].text;
    String chat_id = bot.messages[i].chat_id;

    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }

    // Relay 1
    if (text == "/relay1_on") {
      digitalWrite(relay1, LOW);
      bot.sendMessage(chat_id, "Relay 1 ON", "");
    }
    if (text == "/relay1_off") {
      digitalWrite(relay1, HIGH);
      bot.sendMessage(chat_id, "Relay 1 OFF", "");
    }

    // Relay 2
    if (text == "/relay2_on") {
      digitalWrite(relay2, LOW);
      bot.sendMessage(chat_id, "Relay 2 ON", "");
    }
    if (text == "/relay2_off") {
      digitalWrite(relay2, HIGH);
      bot.sendMessage(chat_id, "Relay 2 OFF", "");
    }

    // Relay 3
    if (text == "/relay3_on") {
      digitalWrite(relay3, LOW);
      bot.sendMessage(chat_id, "Relay 3 ON", "");
    }
    if (text == "/relay3_off") {
      digitalWrite(relay3, HIGH);
      bot.sendMessage(chat_id, "Relay 3 OFF", "");
    }

    // Relay 4
    if (text == "/relay4_on") {
      digitalWrite(relay4, LOW);
      bot.sendMessage(chat_id, "Relay 4 ON", "");
    }
    if (text == "/relay4_off") {
      digitalWrite(relay4, HIGH);
      bot.sendMessage(chat_id, "Relay 4 OFF", "");
    }

    // Help Command
    if (text == "/start") {
      String welcome = "🏠 Home Automation Commands:\n\n";
      welcome += "/relay1_on\n/relay1_off\n";
      welcome += "/relay2_on\n/relay2_off\n";
      welcome += "/relay3_on\n/relay3_off\n";
      welcome += "/relay4_on\n/relay4_off\n";

      bot.sendMessage(chat_id, welcome, "");
    }
  }
}

void loop() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while (numNewMessages) {
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }

  delay(1000);
}