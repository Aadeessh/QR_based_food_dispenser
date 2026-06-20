# QR Food Dispenser System - ESP32 Firebase Integration

A smart automated food dispensing system built with ESP32 that integrates with Firebase Firestore for order management. The system uses ultrasonic sensors, load cells, and servo motors to dispense precise portions of food based on scanned QR codes.

## Features

- QR Code Verification: Scans and validates food orders from Firebase Firestore
- Real-time Weight Monitoring: Uses HX711 load cell for accurate portion control
- Dual Motor Control: Independent control of two DC motors for food dispensing
- Ultrasonic Detection: Automatically detects when containers are placed
- LCD Display: 16x2 I2C LCD shows real-time system status and weight
- Firebase Integration: Cloud-based order management and authentication
- Servo Gating: Servo motor controls food release mechanism
- WiFi Connectivity: Real-time synchronization with cloud database

## Hardware Requirements

### Microcontroller & Connectivity
- ESP32 Development Board
- WiFi Module (built-in)

### Sensors
- HX711 Load Cell Amplifier (Digital Scale Module)
- HC-SR04 Ultrasonic Sensor (Object Detection)
- 2x DC Motor with Speed Control (12V recommended)
- SG90 Servo Motor (Gate Control)

### Display & Interface
- 16x2 I2C LCD Display (Address: 0x27)
- QR Code Scanner (Serial Input)

### Additional Components
- L298N Motor Driver (Dual Motor Control)
- Power Supply (12V for motors, 5V for ESP32)
- Resistors & Capacitors (for stability)
