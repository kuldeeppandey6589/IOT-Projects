#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Preferences.h>

// =====================================================
// Wi-Fi Settings
// =====================================================

const char *ssid = "homeiot";
const char *password = "homeiot123";

// Replace this with your publicly deployed Apps Script URL
String GOOGLE_SCRIPT_URL =
  "https://script.google.com/macros/s/"
  "AKfycbwTlEVkTBctooZUFnXiYKBAwpoLzx_qTQO84YXWKKLmjO6rcNd5PsC1xwUCVc2B5J3C"
  "/exec";

// =====================================================
// OLED Display
// =====================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// =====================================================
// RC522 RFID
// =====================================================

#define SS_PIN 5
#define RST_PIN 4

MFRC522 rfid(SS_PIN, RST_PIN);

// =====================================================
// NTP Time
// =====================================================

WiFiUDP ntpUDP;

NTPClient timeClient(
  ntpUDP,
  "pool.ntp.org",
  19800,     // India: UTC + 5:30
  60000
);

// =====================================================
// Permanent ESP32 Storage
// =====================================================

Preferences preferences;

// Maximum number of users that can be added from Serial Monitor
#define MAX_SAVED_STUDENTS 50

// =====================================================
// Student Structure
// =====================================================

struct Student
{
  String uid;
  String roll;
  String name;
  String studentClass;
};

// =====================================================
// Default Students
// These students are stored in the program
// =====================================================

Student defaultStudents[] =
{
  {"9310B211", "CSE001", "Kuldeep Pandey", "CSE-2"}
};

int totalDefaultStudents =
  sizeof(defaultStudents) / sizeof(defaultStudents[0]);

// =====================================================
// Students Added Through Serial Monitor
// =====================================================

Student savedStudents[MAX_SAVED_STUDENTS];

int savedStudentCount = 0;

// Registration mode
bool registrationMode = false;

// =====================================================
// Function Declarations
// =====================================================

void connectWiFi();
void showReadyScreen();
void showSerialMenu();

void processSerialCommands();
void registerNewCard(String uid);
String readSerialLine(String message);

void loadSavedStudents();
void saveStudent(Student student);
void listAllStudents();
void deleteAllSavedStudents();

bool findStudent(String uid, Student &student);
bool uidAlreadyRegistered(String uid);

void sendToGoogle(Student student);

String getRFIDUID();
String getFormattedDateTime();
String urlEncode(String value);

// =====================================================
// Setup
// =====================================================

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(60000);

  delay(1000);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" ESP32 RFID ATTENDANCE SYSTEM");
  Serial.println("========================================");

  // Start permanent storage
  preferences.begin("rfid-users", false);

  loadSavedStudents();

  // Start OLED
  Wire.begin();

  Serial.println("[OLED] Initializing OLED display...");

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
  {
    Serial.println("[OLED ERROR] OLED display not found.");

    while (true)
    {
      delay(1000);
    }
  }

  Serial.println("[OLED] OLED initialized.");

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.println("RFID Attendance");
  display.println();
  display.println("Starting...");

  display.display();

  // Start RC522
  SPI.begin();

  Serial.println("[RFID] Initializing RC522...");

  rfid.PCD_Init();

  delay(100);

  byte version = rfid.PCD_ReadRegister(rfid.VersionReg);

  Serial.print("[RFID] Version: 0x");
  Serial.println(version, HEX);

  if (version == 0x00 || version == 0xFF)
  {
    Serial.println("[RFID ERROR] RC522 not detected.");
  }
  else
  {
    Serial.println("[RFID] RC522 initialized.");
  }

  Serial.print("[SYSTEM] Default users: ");
  Serial.println(totalDefaultStudents);

  Serial.print("[SYSTEM] Saved users: ");
  Serial.println(savedStudentCount);

  connectWiFi();

  // Start time synchronization
  timeClient.begin();

  Serial.println("[TIME] Synchronizing time...");

  if (timeClient.forceUpdate())
  {
    Serial.print("[TIME] Current time: ");
    Serial.println(getFormattedDateTime());
  }
  else
  {
    Serial.println("[TIME WARNING] NTP synchronization failed.");
  }

  showReadyScreen();
  showSerialMenu();
}

// =====================================================
// Main Loop
// =====================================================

void loop()
{
  timeClient.update();

  // Check Serial Monitor commands
  processSerialCommands();

  // Wait for RFID card
  if (!rfid.PICC_IsNewCardPresent())
  {
    return;
  }

  if (!rfid.PICC_ReadCardSerial())
  {
    return;
  }

  String uid = getRFIDUID();

  Serial.println();
  Serial.println("----------------------------------------");
  Serial.print("[RFID] Scanned UID: ");
  Serial.println(uid);

  // Registration mode
  if (registrationMode)
  {
    registerNewCard(uid);

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();

    delay(1000);

    showReadyScreen();
    return;
  }

  // Normal attendance mode
  Student scannedStudent;

  if (findStudent(uid, scannedStudent))
  {
    Serial.println("[MATCH] Registered user found.");

    Serial.print("[ROLL] ");
    Serial.println(scannedStudent.roll);

    Serial.print("[NAME] ");
    Serial.println(scannedStudent.name);

    Serial.print("[CLASS] ");
    Serial.println(scannedStudent.studentClass);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    display.println("WELCOME");
    display.println();
    display.println(scannedStudent.name);
    display.println();
    display.println("Attendance");
    display.println("Processing...");

    display.display();

    sendToGoogle(scannedStudent);

    delay(3000);
  }
  else
  {
    Serial.println("[UNKNOWN CARD] This card is not registered.");

    Serial.println();
    Serial.println("To register this card:");
    Serial.println("1. Type ADD");
    Serial.println("2. Press Enter");
    Serial.println("3. Scan the card again");

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    display.println("Unknown Card");
    display.println();
    display.println("UID:");
    display.println(uid);
    display.println();
    display.println("Type ADD");

    display.display();

    delay(2500);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  Serial.println("[RFID] Card processing completed.");
  Serial.println("----------------------------------------");

  showReadyScreen();

  delay(1000);
}

// =====================================================
// Serial Monitor Commands
// =====================================================

void processSerialCommands()
{
  if (!Serial.available())
  {
    return;
  }

  String command = Serial.readStringUntil('\n');

  command.trim();
  command.toUpperCase();

  if (command.length() == 0)
  {
    return;
  }

  if (command == "ADD")
  {
    registrationMode = true;

    Serial.println();
    Serial.println("========================================");
    Serial.println(" NEW CARD REGISTRATION MODE");
    Serial.println("========================================");
    Serial.println("Now scan the new RFID card.");
    Serial.println("Type CANCEL to stop registration.");

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    display.println("ADD NEW USER");
    display.println();
    display.println("Scan New Card...");
    display.println();
    display.println("Waiting for card");

    display.display();
  }
  else if (command == "LIST")
  {
    listAllStudents();
  }
  else if (command == "DELETEALL")
  {
    deleteAllSavedStudents();
  }
  else if (command == "CANCEL")
  {
    registrationMode = false;

    Serial.println("[REGISTRATION] Registration cancelled.");

    showReadyScreen();
  }
  else if (command == "HELP")
  {
    showSerialMenu();
  }
  else
  {
    Serial.println("[COMMAND ERROR] Unknown command.");
    showSerialMenu();
  }
}

// =====================================================
// Register New RFID Card
// =====================================================

void registerNewCard(String uid)
{
  Serial.println();
  Serial.println("========================================");
  Serial.println(" CARD DETECTED");
  Serial.println("========================================");

  Serial.print("New card UID: ");
  Serial.println(uid);

  if (uidAlreadyRegistered(uid))
  {
    Serial.println("[ERROR] This card is already registered.");

    display.clearDisplay();
    display.setCursor(0, 0);

    display.println("Card Already");
    display.println("Registered");
    display.println();
    display.println("UID:");
    display.println(uid);

    display.display();

    registrationMode = false;

    delay(2500);
    return;
  }

  if (savedStudentCount >= MAX_SAVED_STUDENTS)
  {
    Serial.println("[ERROR] Student storage is full.");

    display.clearDisplay();
    display.setCursor(0, 0);

    display.println("Storage Full");
    display.println();
    display.println("Cannot Add User");

    display.display();

    registrationMode = false;

    delay(2500);
    return;
  }

  Serial.println();
  Serial.println("Enter the following student details.");
  Serial.println("Type each value and press Enter.");
  Serial.println();

  Student newStudent;

  newStudent.uid = uid;

  newStudent.roll =
    readSerialLine("Enter Roll Number: ");

  newStudent.name =
    readSerialLine("Enter Student Name: ");

  newStudent.studentClass =
    readSerialLine("Enter Class: ");

  if (
    newStudent.roll.length() == 0 ||
    newStudent.name.length() == 0 ||
    newStudent.studentClass.length() == 0
  )
  {
    Serial.println("[ERROR] Empty information is not allowed.");
    Serial.println("[REGISTRATION] User was not saved.");

    registrationMode = false;
    return;
  }

  saveStudent(newStudent);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" NEW USER SAVED SUCCESSFULLY");
  Serial.println("========================================");

  Serial.print("UID: ");
  Serial.println(newStudent.uid);

  Serial.print("Roll Number: ");
  Serial.println(newStudent.roll);

  Serial.print("Name: ");
  Serial.println(newStudent.name);

  Serial.print("Class: ");
  Serial.println(newStudent.studentClass);

  Serial.println("The data is stored permanently in ESP32.");
  Serial.println("========================================");

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.println("USER ADDED");
  display.println();
  display.println(newStudent.name);
  display.println();
  display.println(newStudent.roll);
  display.println(newStudent.studentClass);

  display.display();

  registrationMode = false;

  delay(3000);
}

// =====================================================
// Read Information From Serial Monitor
// =====================================================

String readSerialLine(String message)
{
  Serial.print(message);

  while (true)
  {
    if (Serial.available())
    {
      String value = Serial.readStringUntil('\n');

      value.trim();

      if (value.length() > 0)
      {
        Serial.println(value);
        return value;
      }

      Serial.println("[ERROR] Value cannot be empty.");
      Serial.print(message);
    }

    delay(10);
  }
}

// =====================================================
// Save Student Permanently
// =====================================================

void saveStudent(Student student)
{
  int index = savedStudentCount;

  savedStudents[index] = student;

  preferences.putString(
    ("uid" + String(index)).c_str(),
    student.uid
  );

  preferences.putString(
    ("roll" + String(index)).c_str(),
    student.roll
  );

  preferences.putString(
    ("name" + String(index)).c_str(),
    student.name
  );

  preferences.putString(
    ("class" + String(index)).c_str(),
    student.studentClass
  );

  savedStudentCount++;

  preferences.putInt("count", savedStudentCount);
}

// =====================================================
// Load Saved Students After Restart
// =====================================================

void loadSavedStudents()
{
  savedStudentCount = preferences.getInt("count", 0);

  if (savedStudentCount < 0)
  {
    savedStudentCount = 0;
  }

  if (savedStudentCount > MAX_SAVED_STUDENTS)
  {
    savedStudentCount = MAX_SAVED_STUDENTS;
  }

  for (int i = 0; i < savedStudentCount; i++)
  {
    savedStudents[i].uid =
      preferences.getString(
        ("uid" + String(i)).c_str(),
        ""
      );

    savedStudents[i].roll =
      preferences.getString(
        ("roll" + String(i)).c_str(),
        ""
      );

    savedStudents[i].name =
      preferences.getString(
        ("name" + String(i)).c_str(),
        ""
      );

    savedStudents[i].studentClass =
      preferences.getString(
        ("class" + String(i)).c_str(),
        ""
      );
  }

  Serial.print("[MEMORY] Loaded saved users: ");
  Serial.println(savedStudentCount);
}

// =====================================================
// Find Student by RFID UID
// =====================================================

bool findStudent(String uid, Student &student)
{
  // Search default users
  for (int i = 0; i < totalDefaultStudents; i++)
  {
    if (uid == defaultStudents[i].uid)
    {
      student = defaultStudents[i];
      return true;
    }
  }

  // Search users stored in ESP32 memory
  for (int i = 0; i < savedStudentCount; i++)
  {
    if (uid == savedStudents[i].uid)
    {
      student = savedStudents[i];
      return true;
    }
  }

  return false;
}

// =====================================================
// Check for Duplicate Card
// =====================================================

bool uidAlreadyRegistered(String uid)
{
  Student student;

  return findStudent(uid, student);
}

// =====================================================
// List All Registered Students
// =====================================================

void listAllStudents()
{
  Serial.println();
  Serial.println("========================================");
  Serial.println(" ALL REGISTERED USERS");
  Serial.println("========================================");

  Serial.println();
  Serial.println("DEFAULT USERS:");

  for (int i = 0; i < totalDefaultStudents; i++)
  {
    Serial.print(i + 1);
    Serial.print(". UID: ");
    Serial.print(defaultStudents[i].uid);

    Serial.print(" | Roll: ");
    Serial.print(defaultStudents[i].roll);

    Serial.print(" | Name: ");
    Serial.print(defaultStudents[i].name);

    Serial.print(" | Class: ");
    Serial.println(defaultStudents[i].studentClass);
  }

  Serial.println();
  Serial.println("SERIAL-MONITOR ADDED USERS:");

  if (savedStudentCount == 0)
  {
    Serial.println("No users added through Serial Monitor.");
  }
  else
  {
    for (int i = 0; i < savedStudentCount; i++)
    {
      Serial.print(i + 1);
      Serial.print(". UID: ");
      Serial.print(savedStudents[i].uid);

      Serial.print(" | Roll: ");
      Serial.print(savedStudents[i].roll);

      Serial.print(" | Name: ");
      Serial.print(savedStudents[i].name);

      Serial.print(" | Class: ");
      Serial.println(savedStudents[i].studentClass);
    }
  }

  Serial.println("========================================");
}

// =====================================================
// Delete All Serial-Monitor Added Users
// =====================================================

void deleteAllSavedStudents()
{
  Serial.println();
  Serial.println("[WARNING] Deleting all saved users...");

  preferences.clear();

  savedStudentCount = 0;

  for (int i = 0; i < MAX_SAVED_STUDENTS; i++)
  {
    savedStudents[i].uid = "";
    savedStudents[i].roll = "";
    savedStudents[i].name = "";
    savedStudents[i].studentClass = "";
  }

  Serial.println("[MEMORY] All Serial-Monitor users deleted.");
  Serial.println("[MEMORY] Default code users remain available.");

  display.clearDisplay();
  display.setCursor(0, 0);

  display.println("Saved Users");
  display.println("Deleted");
  display.println();
  display.println("Default Users");
  display.println("Still Available");

  display.display();

  delay(2500);

  showReadyScreen();
}

// =====================================================
// Connect to Wi-Fi
// =====================================================

void connectWiFi()
{
  Serial.println();
  Serial.print("[WIFI] Connecting to: ");
  Serial.println(ssid);

  display.clearDisplay();
  display.setCursor(0, 0);

  display.println("Connecting WiFi");
  display.println();
  display.println(ssid);

  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempt = 0;

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");

    attempt++;

    if (attempt >= 30)
    {
      Serial.println();
      Serial.println("[WIFI] Retrying connection...");

      WiFi.disconnect();
      delay(1000);
      WiFi.begin(ssid, password);

      attempt = 0;
    }
  }

  Serial.println();
  Serial.println("[WIFI] Connected.");

  Serial.print("[WIFI] IP address: ");
  Serial.println(WiFi.localIP());

  display.clearDisplay();
  display.setCursor(0, 0);

  display.println("WiFi Connected");
  display.println();
  display.println(WiFi.localIP());

  display.display();

  delay(2000);
}

// =====================================================
// Ready Screen
// =====================================================

void showReadyScreen()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.println("RFID Attendance");
  display.println();
  display.println("Scan Card...");
  display.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    display.println("WiFi: Connected");
  }
  else
  {
    display.println("WiFi: Offline");
  }

  display.print("Time: ");
  display.println(timeClient.getFormattedTime());

  display.display();
}

// =====================================================
// Show Available Commands
// =====================================================

void showSerialMenu()
{
  Serial.println();
  Serial.println("========================================");
  Serial.println(" SERIAL MONITOR COMMANDS");
  Serial.println("========================================");
  Serial.println("ADD       : Add a new RFID user");
  Serial.println("LIST      : Show registered users");
  Serial.println("DELETEALL : Delete Serial-added users");
  Serial.println("CANCEL    : Cancel registration");
  Serial.println("HELP      : Show commands");
  Serial.println("========================================");
}

// =====================================================
// Read RFID UID
// =====================================================

String getRFIDUID()
{
  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++)
  {
    if (rfid.uid.uidByte[i] < 0x10)
    {
      uid += "0";
    }

    uid += String(rfid.uid.uidByte[i], HEX);
  }

  uid.toUpperCase();

  return uid;
}

// =====================================================
// Send Attendance to Google Sheets
// =====================================================

void sendToGoogle(Student student)
{
  Serial.println();
  Serial.println("[GOOGLE] Preparing attendance data...");

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[GOOGLE ERROR] Wi-Fi disconnected.");

    display.clearDisplay();
    display.setCursor(0, 0);

    display.println("WiFi Error");
    display.println();
    display.println("Attendance Not");
    display.println("Submitted");

    display.display();

    return;
  }

  HTTPClient http;

  String url = GOOGLE_SCRIPT_URL;

  url += "?roll=" + urlEncode(student.roll);
  url += "&name=" + urlEncode(student.name);
  url += "&class=" + urlEncode(student.studentClass);
  url += "&status=Present";
  url += "&time=" + urlEncode(timeClient.getFormattedTime());

  Serial.print("[GOOGLE] URL: ");
  Serial.println(url);

  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);

  if (!http.begin(url))
  {
    Serial.println("[HTTP ERROR] HTTP connection failed.");
    return;
  }

  int responseCode = http.GET();

  Serial.print("[HTTP] Response code: ");
  Serial.println(responseCode);

  if (responseCode > 0)
  {
    String response = http.getString();

    Serial.print("[HTTP] Response: ");
    Serial.println(response);

    if (responseCode == 200)
    {
      display.clearDisplay();
      display.setCursor(0, 0);

      display.println("Attendance Done");
      display.println();
      display.println(student.name);
      display.println();
      display.println("Status: Present");
      display.println(timeClient.getFormattedTime());

      display.display();

      Serial.println("[ATTENDANCE] Submitted successfully.");
    }
    else
    {
      display.clearDisplay();
      display.setCursor(0, 0);

      display.println("Server Error");
      display.println();
      display.print("Code: ");
      display.println(responseCode);
      display.println();
      display.println("Check Deployment");

      display.display();
    }
  }
  else
  {
    Serial.print("[HTTP ERROR] ");
    Serial.println(http.errorToString(responseCode));

    display.clearDisplay();
    display.setCursor(0, 0);

    display.println("Upload Failed");
    display.println();
    display.println("Check Internet");

    display.display();
  }

  http.end();
}

// =====================================================
// Get Time
// =====================================================

String getFormattedDateTime()
{
  if (timeClient.getEpochTime() < 100000)
  {
    return "Time not synchronized";
  }

  return timeClient.getFormattedTime();
}

// =====================================================
// URL Encode
// =====================================================

String urlEncode(String value)
{
  String encodedValue = "";

  const char hexCharacters[] = "0123456789ABCDEF";

  for (unsigned int i = 0; i < value.length(); i++)
  {
    unsigned char character = value.charAt(i);

    if (
      (character >= 'a' && character <= 'z') ||
      (character >= 'A' && character <= 'Z') ||
      (character >= '0' && character <= '9') ||
      character == '-' ||
      character == '_' ||
      character == '.' ||
      character == '~'
    )
    {
      encodedValue += (char)character;
    }
    else
    {
      encodedValue += '%';
      encodedValue += hexCharacters[(character >> 4) & 0x0F];
      encodedValue += hexCharacters[character & 0x0F];
    }
  }

  return encodedValue;
}