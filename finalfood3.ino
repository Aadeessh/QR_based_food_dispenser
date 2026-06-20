#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <HX711_ADC.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Firebase Configuration
#define FIREBASE_PROJECT_ID "***********"
#define FIREBASE_API_KEY "*****************"

// Firebase objects
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// WiFi Credentials
const char* ssid = "Pavi";
const char* password = "123456789";

// Firebase Authentication Credentials
const char* firebaseEmail = "thefdcompany03@gmail.com";
const char* firebasePassword = "******";

// Define LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Define motor control pins for two motors
#define IN1 32
#define IN2 33
#define ENA 14
#define IN3 27
#define IN4 26
#define ENB 12

// Define ultrasonic sensor
#define TRIG_PIN 16
#define ECHO_PIN 17

// Define load cell
#define DT 4
#define SCK 5
HX711_ADC LoadCell(DT, SCK);

// Define servos
#define SERVO_PIN 25
#define SERVO2_PIN 23
Servo myServo;
Servo myServo2;

// Variables for weight
float calibrationValue = 210.99;
float weight = 0;

// Variables for continuous servo control
unsigned long lastDirectionChange = 0;
int servo2Direction = 100;

void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\n✅ Connected to WiFi!");

  config.api_key = FIREBASE_API_KEY;
  auth.user.email = firebaseEmail;
  auth.user.password = firebasePassword;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (Firebase.ready()) {
    Serial.println("✅ Firebase authentication successful.");
  } else {
    Serial.println("❌ Firebase authentication failed.");
    return;
  }

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  myServo.attach(SERVO_PIN);
  myServo2.attach(SERVO2_PIN);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  LoadCell.begin();
  LoadCell.start(1000, true);
  LoadCell.setCalFactor(calibrationValue);

  Serial.println("✅ System Ready. Waiting for QR code...");
}

long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.034 / 2;
}

void performTare() {
  Serial.println("⚖️  Performing Tare...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Taring...");
  LoadCell.tare();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tare Complete");
  Serial.println("✅ Tare Completed Successfully!");
  delay(1000);
  lcd.clear();
}

void startMotors() {
  Serial.println("🏎️ Motors Started...");
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 255);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENB, 255);
}

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
        if (foodItem == "Curd Rice" || foodItem == "Tomato Rice" || foodItem == "Kuska") {
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
  if (distance >= 0 && distance <= 23) {
    Serial.println("🎯 Object Detected. Starting Process...");

    performTare();
    weight = 0;

    startMotors();
    myServo.write(90);

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

      if (millis() - lastDirectionChange > 1000) {
        lastDirectionChange = millis();
        servo2Direction = (servo2Direction == 80) ? 100 : 80;
        myServo2.write(servo2Direction);
      }

      delay(100);
      myServo2.write(90);
      delay(100);
    }

    stopMotors();
    myServo.write(180);
    myServo2.write(90);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Weight Reached");
    Serial.println("✅ Weight Limit Reached. Motors Stopped.");

    weight = 0;
  } else {
    Serial.println("⚠️ No object detected. Waiting...");
  }
}
