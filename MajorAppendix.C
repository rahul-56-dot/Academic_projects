/************************************************************
 * SMART HOSPITAL ROOM MONITORING SYSTEM - ONLINE (BLYNK) VERSION
 * Sensors: DHT22, MQ135, MAX30102, IR Sensor (Fire)
 * Peripherals: OLED Display, Buzzer
 * Platform: ESP32 + Blynk Cloud
 ************************************************************/
#define BLYNK_TEMPLATE_ID "TMPL3CVqXPIHr"
#define BLYNK_TEMPLATE_NAME "Smart Automated Hospital Management System"
#define BLYNK_AUTH_TOKEN "mMKc3UgdPbN5YDvyEYE86QwD0SuYqSNr"
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include "DHT.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// ---------- WiFi Credentials ----------
char ssid[] = "hospproj";
char pass[] = "987654312";
// ---------- Define Room ----------
#define ROOM_NUMBER "Room 102"
// ---------- Sensor Pins ----------
#define DHTPIN 32
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
#define MQ135_PIN 33
#define IR_SENSOR_PIN 25
#define BUZZER_PIN 26
// I2C Pins
#define SDA_PIN 4
#define SCL_PIN 5
MAX30105 particleSensor;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
// ---------- Thresholds ----------
float tempThreshold = 30.0;
int airQualityThreshold = 300;
int heartRateLow = 60;
int heartRateHigh = 70;
int spo2Threshold = 94;
// ---------- Variables ----------
float temperature, humidity;
int airQuality;
int heartRate = 0;
int spo2 = 0;
bool irDetected = false;
int irState = HIGH;
// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(1000);
  // Initialize Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  // Initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  dht.begin();
  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  // OLED Display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found!");
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 25);
  display.println("SMART HOSPITAL INIT...");
  display.display();
  delay(2000);
  // MAX30102 Setup
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD, 0x57)) {
    Serial.println("MAX30102 not found!");
    display.clearDisplay();
    display.setCursor(0, 25);
    display.println("MAX30102 ERROR!");
    display.display();
    while (1);
  }
  particleSensor.setup();
  display.clearDisplay();
  display.setCursor(10, 25);
  display.println("SYSTEM READY");
  display.display();
  delay(2000);
}
// ---------- Main Loop ----------
void loop() {
  Blynk.run();
  // --- Read Sensors ---
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  irState = digitalRead(IR_SENSOR_PIN);
  irDetected = (irState == LOW);
  airQuality = analogRead(MQ135_PIN);
  long irValue = particleSensor.getIR();
  long redValue = particleSensor.getRed();
  if (irValue < 50000) {
    heartRate = 0;
    spo2 = 0;
  } else {
    heartRate = map(redValue % 1000, 0, 1000, 60, 100);
    spo2 = map(irValue % 1000, 0, 1000, 90, 99);
  }
  // --- OLED Display ---
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("SMART HOSPITAL");
  display.setCursor(0, 12);
  display.print("Temp: "); display.print(temperature, 1); display.println(" C");
  display.setCursor(0, 24);
  display.print("Humidity: "); display.print(humidity, 1); display.println(" %");
  display.setCursor(0, 36);
  display.print("HR: "); display.print(heartRate); display.println(" bpm");
  display.setCursor(0, 48);
  display.print("SpO2: "); display.print(spo2); display.println(" %");
  display.setCursor(0, 58);
  display.print("Fire: "); display.println(irDetected ? "Detected" : "Clear");
  display.display();
  // --- Send Data to Blynk ---
  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);
  Blynk.virtualWrite(V2, airQuality);
  Blynk.virtualWrite(V3, irDetected ? 1 : 0);
  Blynk.virtualWrite(V4, heartRate);
  Blynk.virtualWrite(V5, spo2);
  Blynk.virtualWrite(V6, 1); // Vin Status ON

  // --- Serial Output ---
  Serial.println("-------------------------------------------------");
  Serial.println(ROOM_NUMBER);
  Serial.print("Temp: "); Serial.print(temperature); Serial.println(" °C");
  Serial.print("Humidity: "); Serial.print(humidity); Serial.println(" %");
  Serial.print("Air Quality: "); Serial.println(airQuality);
  Serial.print("Heart Rate: "); Serial.print(heartRate); Serial.println(" bpm");
  Serial.print("SpO2: "); Serial.print(spo2); Serial.println(" %");
  Serial.print("Fire: "); Serial.println(irDetected ? "Detected" : "Clear");
  Serial.println("-------------------------------------------------");

  // --- Alerts ---
  bool alert = false;
  if (temperature > tempThreshold) {
    Blynk.logEvent("temp_alert", String(ROOM_NUMBER) + " High Temp: " + temperature + "°C");
    alert = true;
  }
  if (humidity > 80) {
    Blynk.logEvent("humidity_alert", String(ROOM_NUMBER) + " High Humidity: " + humidity + "%");
    alert = true;
  }
  if (irDetected) {
    Blynk.logEvent("fire_alert", String(ROOM_NUMBER) + " Fire detected!");
    alert = true;
  }
  if (heartRate < heartRateLow || heartRate > heartRateHigh) {
    Blynk.logEvent("hr_alert", String(ROOM_NUMBER) + " Abnormal Heart Rate: " + heartRate + " bpm");
    alert = true;
  }
  if (spo2 < spo2Threshold) {
    Blynk.logEvent("spo2_alert", String(ROOM_NUMBER) + " Low SpO2: " + spo2 + "%");
    alert = true;
  }
  // --- Buzzer ---
  if (alert) {
    tone(BUZZER_PIN, 1000);
    delay(300);
    noTone(BUZZER_PIN);
  } else {
    noTone(BUZZER_PIN);
  }
  delay(3000);
}
