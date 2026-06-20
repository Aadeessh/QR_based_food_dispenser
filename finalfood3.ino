#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <HX711_ADC.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Firebase Configuration
#define FIREBASE_PROJECT_ID "food-dispenser-22913"
#define FIREBASE_API_KEY "AIzaSyCV-Id-4iBVtJ4hwDkO2VLG11smupJnm34"

// Firebase objects
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// WiFi Credentials
const char* ssid = "Aadeessh";
const char* password = "19102005";

// Firebase Authentication Credentials
const char* firebaseEmail = "thefdcompany03@gmail.com";
const char* firebasePassword = "Hepy@8075";

// Define LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Define motor control pins for two motors
#define IN1 32  // Motor 1 Forward
#define IN2 33  // Motor 1 Reverse
#define ENA 14  // Motor 1 Speed Control

#define IN3 27  // Motor 2 Forward
#define IN4 26  // Motor 2 Reverse
#define ENB 12  // Motor 2 Speed Control

// Define ultrasonic sensor
#define TRIG_PIN 16
#define ECHO_PIN 17

// Define load cell
#define DT 4
#define SCK 5
HX711_ADC LoadCell(DT, SCK);

// Define servo
#define SERVO_PIN 25
Servo myServo;

// Variables for weight
float calibrationValue = 210.99;
unsigned long t = 0;
float weight = 0;

void setup() {
  Serial.begin(115200);

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");

  // Connect to WiFi
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\n✅ Connected to WiFi!");

  // Set Firebase Configuration
  config.api_key = FIREBASE_API_KEY;
  auth.user.email = firebaseEmail;
  auth.user.password = firebasePassword;

  // Initialize Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (Firebase.ready()) {
    Serial.println("✅ Firebase authentication successful.");
  } else {
    Serial.println("❌ Firebase authentication failed.");
    return;
  }

  // Initialize motor and servo
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  myServo.attach(SERVO_PIN);

  // Initialize ultrasonic sensor
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Initialize load cell
  LoadCell.begin();
  LoadCell.start(1000, true);
  LoadCell.setCalFactor(calibrationValue);

  Serial.println("✅ System Ready. Waiting for QR code...");
}

// Function to get distance using ultrasonic sensor
long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.034 / 2;
}

// Function to perform tare and reset weight to zero
void performTare() {
  Serial.println("⚖️  Performing Tare...");
  LoadCell.tareNoDelay();  // Start tare without blocking
  unsigned long tareTimeout = millis();

  while (!LoadCell.update()) {
    if (millis() - tareTimeout > 2000) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Tare Failed!");
      Serial.println("❌ Tare Failed!");
      delay(1000);
      return;
    }
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tare Complete");
  Serial.println("✅ Tare Completed Successfully!");
  delay(1000);
  lcd.clear();
}

// Function to start motors
void startMotors() {
  Serial.println("🏎️ Motors Started...");

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 255); // Motor 1 Speed

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENB, 255); // Motor 2 Speed
}

// Function to stop motors
void stopMotors() {
  Serial.println("⏹️ Stopping Motors...");

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

void loop() {
  Serial.println("\n📡 Waiting for QR code data...");
  while (!Serial.available());
  String qrData = Serial.readStringUntil('\n');
  qrData.trim();
  Serial.println("🔍 Scanned QR Code: " + qrData);

  String path = "/orders/" + qrData;
  Serial.println("📡 Fetching data from Firestore...");

  if (Firebase.Firestore.getDocument(&firebaseData, FIREBASE_PROJECT_ID, "", path.c_str())) {
    if (firebaseData.httpCode() == FIREBASE_ERROR_HTTP_CODE_OK) {
      String jsonData = firebaseData.payload();
      Serial.println("✅ Firestore Data Received:");
      Serial.println(jsonData);

      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, jsonData);

      if (error) {
        Serial.println("❌ Failed to parse JSON");
        return;
      }

      JsonArray foodItems = doc["fields"]["food_items"]["arrayValue"]["values"].as<JsonArray>();
      bool itemFound = false;

      for (JsonVariant v : foodItems) {
        String foodItem = v["mapValue"]["fields"]["name"]["stringValue"].as<String>();
        Serial.println("🍲 Checking item: " + foodItem);

        if (foodItem == "Curd Rice" || foodItem == "Tomato rice" || foodItem == "Kuska") {
          itemFound = true;
          Serial.println("✅ " + foodItem + " found. Proceeding...");
          break;
        }
      }

      if (!itemFound) {
        Serial.println("❌ Food item not matched. Access denied.");
        return;
      }
    } else {
      Serial.println("❌ Firestore document not found!");
      return;
    }
  } else {
    Serial.println("🔥 Firebase Firestore Error: " + String(firebaseData.errorReason().c_str()));
    return;
  }

  delay(1000);

  long distance = getDistance();
  if (distance > 0 && distance <= 20) {
    Serial.println("🎯 Object Detected. Starting Process...");
    performTare();

    startMotors();
    myServo.write(180);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Running...");
    delay(2000);

    while (weight < 195) {
      if (LoadCell.update()) {
        weight = abs(LoadCell.getData());
        Serial.println("⚖️  Weight: " + String(weight) + " g");

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Weight: " + String(weight) + " g");
      }
    }

    stopMotors();
    myServo.write(0);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Weight Limit Reached");
    Serial.println("✅ Weight Limit Reached. Motors Stopped.");
  } else {
    Serial.println("⚠️ No object detected. Waiting...");
  }
}
