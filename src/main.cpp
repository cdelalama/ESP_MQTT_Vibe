/*
* Version: 1.0.0
* ESP32-H2 Boiler Vibration Monitoring System
* Connects to WiFi and MQTT broker, reads ADXL345 accelerometer data
* to detect boiler status based on vibration level.
* Supports OTA updates.
*/

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_ADXL345_U.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <AsyncElegantOTA.h>

// WiFi credentials
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";

// MQTT configuration
const char *mqtt_server = "YOUR_MQTT_BROKER_IP";
const int mqtt_port = 1883;
const char *mqtt_user = "YOUR_MQTT_USERNAME";     // Optional, comment if not needed
const char *mqtt_password = "YOUR_MQTT_PASSWORD"; // Optional, comment if not needed
const char *mqtt_client_id = "ESP32H2_Boiler";
const char *mqtt_topic_status = "boiler/status";
const char *mqtt_topic_vibration = "boiler/vibration";
const char *mqtt_topic_sensitivity = "boiler/sensitivity/set";

// ADXL345 accelerometer
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// WiFi and MQTT clients
WiFiClient espClient;
PubSubClient mqttClient(espClient);
AsyncWebServer server(80);

// Variables for vibration detection
float vibrationThreshold = 1.0;  // Default sensitivity (can be adjusted via MQTT)
bool boilerStatus = false;        // Current boiler status (ON/OFF)
unsigned long lastReading = 0;    // Last time the sensor was read
const int readingInterval = 1000; // Reading interval in milliseconds
const int reportInterval = 10000; // Status reporting interval in milliseconds
unsigned long lastReport = 0;     // Last time the status was reported

// Function prototypes
void setupWiFi();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void setupOTA();
bool detectVibration();
void publishStatus();

void setup() {
  Serial.begin(115200);
  Serial.println("Boiler Vibration Monitoring System Starting...");

  // Initialize I2C for ADXL345
  Wire.begin();

  // Initialize ADXL345
  if (!accel.begin()) {
    Serial.println("Could not find a valid ADXL345 sensor, check wiring!");
    while (1);
  }

  // Set the range to +-2g
  accel.setRange(ADXL345_RANGE_2_G);

  // Setup WiFi
  setupWiFi();

  // Setup MQTT
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  // Setup OTA
  setupOTA();

  Serial.println("System initialized and ready!");
}

void loop() {
  // Ensure WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    setupWiFi();
  }

  // Ensure MQTT is connected
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  // Read accelerometer data and detect vibration
  unsigned long currentMillis = millis();
  if (currentMillis - lastReading >= readingInterval) {
    lastReading = currentMillis;

    // Get vibration state
    bool currentBoilerState = detectVibration();

    // If state changed, publish immediately
    if (currentBoilerState != boilerStatus) {
      boilerStatus = currentBoilerState;
      publishStatus();
      lastReport = currentMillis;
    }
    // Periodically report status even if unchanged
    else if (currentMillis - lastReport >= reportInterval) {
      publishStatus();
      lastReport = currentMillis;
    }
  }
}

void setupWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected to WiFi. IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("Failed to connect to WiFi. Will retry later.");
  }
}

void reconnectMQTT() {
  // Loop until we're reconnected to the MQTT broker
  int attempts = 0;
  while (!mqttClient.connected() && attempts < 5) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect with authentication
    if (mqttClient.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
      Serial.println("connected");

      // Subscribe to the sensitivity setting topic
      mqttClient.subscribe(mqtt_topic_sensitivity);

      // Publish initial status
      publishStatus();
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
    attempts++;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  char message[length + 1];
  for (unsigned int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';

  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  Serial.println(message);

  // Check if it's the sensitivity setting topic
  if (String(topic) == mqtt_topic_sensitivity) {
    float newThreshold = atof(message);
    if (newThreshold > 0) {
      vibrationThreshold = newThreshold;
      Serial.print("Sensitivity threshold updated to: ");
      Serial.println(vibrationThreshold);

      // Acknowledge the change
      String ackTopic = String(mqtt_topic_sensitivity) + "/ack";
      mqttClient.publish(ackTopic.c_str(), message);
    }
  }
}

void setupOTA() {
  // Initialize AsyncWebServer
  AsyncElegantOTA.begin(&server);
  server.begin();

  Serial.println("HTTP server started for OTA updates");
  Serial.print("OTA available at http://");
  Serial.print(WiFi.localIP());
  Serial.println("/update");
}

bool detectVibration() {
  // Read accelerometer
  sensors_event_t event;
  accel.getEvent(&event);

  // Calculate magnitude of acceleration (remove gravity)
  float x = event.acceleration.x;
  float y = event.acceleration.y;
  float z = event.acceleration.z - 9.8; // Subtract gravity (approximate)

  float magnitude = sqrt(x*x + y*y + z*z);

  // Publish vibration level for monitoring
  char vibStr[10];
  dtostrf(magnitude, 5, 2, vibStr);
  mqttClient.publish(mqtt_topic_vibration, vibStr);

  Serial.print("Vibration magnitude: ");
  Serial.println(magnitude);

  // Determine boiler status based on vibration threshold
  return magnitude > vibrationThreshold;
}

void publishStatus() {
  const char* status = boilerStatus ? "ON" : "OFF";
  mqttClient.publish(mqtt_topic_status, status);

  Serial.print("Published boiler status: ");
  Serial.println(status);
}