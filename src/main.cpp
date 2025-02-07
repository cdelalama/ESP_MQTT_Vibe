/*
* Version: 1.2.0
* ESP32-H2 Boiler Vibration Monitoring System
* Connects to WiFi and MQTT broker, reads ADXL345 accelerometer data
* to detect boiler status based on vibration level.
* Supports OTA updates and web configuration.
*/

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_ADXL345_U.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <AsyncElegantOTA.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Default values (will be overridden by .env if available)
String wifi_ssid = "";
String wifi_password = "";
String mqtt_server = "";
int mqtt_port = 1883;
String mqtt_user = "";
String mqtt_password = "";
String mqtt_client_id = "ESP32H2_Boiler";
float vibrationThreshold = 1.0;

// MQTT topics
const char *mqtt_topic_status = "boiler/status";
const char *mqtt_topic_vibration = "boiler/vibration";
const char *mqtt_topic_sensitivity = "boiler/sensitivity/set";
const char *mqtt_topic_sensor_health = "boiler/sensor/health";

// ADXL345 accelerometer
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
bool sensorActive = false;

// WiFi and MQTT clients
WiFiClient espClient;
PubSubClient mqttClient(espClient);
AsyncWebServer server(80);

// Variables for vibration detection
bool boilerStatus = false;        // Current boiler status (ON/OFF)
unsigned long lastReading = 0;    // Last time the sensor was read
const int readingInterval = 1000; // Reading interval in milliseconds
const int reportInterval = 10000; // Status reporting interval in milliseconds
unsigned long lastReport = 0;     // Last time the status was reported
unsigned long lastSensorCheck = 0; // Last time the sensor was checked

// Function prototypes
void loadConfigFromEnv();
bool saveConfigToEnv(const String &configJson);
void setupWiFi();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void setupOTA();
bool detectVibration();
void publishStatus();
void checkSensor();
String getValue(String data, char separator, int index);
void handleWebConfig();

void setup() {
  Serial.begin(115200);
  Serial.println("Boiler Vibration Monitoring System Starting...");

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Failed to mount LittleFS");
  } else {
    Serial.println("LittleFS mounted successfully");
    // Load configuration from env.txt
    loadConfigFromEnv();
  }

  // Initialize I2C for ADXL345
  Wire.begin();

  // Try to initialize ADXL345, but don't block if it fails
  checkSensor();

  // Setup WiFi
  setupWiFi();

  // Setup MQTT
  mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
  mqttClient.setCallback(mqttCallback);

  // Setup OTA and Web Configuration
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

  // Periodically check sensor health
  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorCheck >= 30000) { // Check every 30 seconds
    lastSensorCheck = currentMillis;
    checkSensor();
  }

  // Read accelerometer data and detect vibration (if sensor is active)
  if (currentMillis - lastReading >= readingInterval) {
    lastReading = currentMillis;

    if (sensorActive) {
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
    } else {
      // If sensor is not active, still report periodically
      if (currentMillis - lastReport >= reportInterval) {
        publishStatus();
        lastReport = currentMillis;
      }
    }
  }
}

void loadConfigFromEnv() {
  if (LittleFS.exists("/env.txt")) {
    File file = LittleFS.open("/env.txt", "r");
    if (file) {
      Serial.println("Reading configuration from env.txt");

      while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();

        // Skip comments and empty lines
        if (line.startsWith("#") || line.length() == 0) {
          continue;
        }

        // Parse key=value pairs
        int separatorPos = line.indexOf('=');
        if (separatorPos > 0) {
          String key = line.substring(0, separatorPos);
          String value = line.substring(separatorPos + 1);

          if (key == "WIFI_SSID") {
            wifi_ssid = value;
            Serial.println("WIFI_SSID loaded");
          } else if (key == "WIFI_PASSWORD") {
            wifi_password = value;
            Serial.println("WIFI_PASSWORD loaded");
          } else if (key == "MQTT_SERVER") {
            mqtt_server = value;
            Serial.println("MQTT_SERVER loaded");
          } else if (key == "MQTT_PORT") {
            mqtt_port = value.toInt();
            Serial.println("MQTT_PORT loaded");
          } else if (key == "MQTT_USER") {
            mqtt_user = value;
            Serial.println("MQTT_USER loaded");
          } else if (key == "MQTT_PASSWORD") {
            mqtt_password = value;
            Serial.println("MQTT_PASSWORD loaded");
          } else if (key == "MQTT_CLIENT_ID") {
            mqtt_client_id = value;
            Serial.println("MQTT_CLIENT_ID loaded");
          } else if (key == "VIBRATION_THRESHOLD") {
            vibrationThreshold = value.toFloat();
            Serial.println("VIBRATION_THRESHOLD loaded: " + String(vibrationThreshold));
          }
        }
      }

      file.close();
    } else {
      Serial.println("Failed to open env.txt for reading");
    }
  } else {
    Serial.println("env.txt not found. Using default values.");
  }
}

bool saveConfigToEnv(const String &configJson) {
  // Parse JSON
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, configJson);

  // Create a buffer for writing to file
  String fileContent = "";

  // WiFi Configuration
  fileContent += "# WiFi Configuration\n";
  fileContent += "WIFI_SSID=" + doc["wifi_ssid"].as<String>() + "\n";
  fileContent += "WIFI_PASSWORD=" + doc["wifi_password"].as<String>() + "\n\n";

  // MQTT Configuration
  fileContent += "# MQTT Configuration\n";
  fileContent += "MQTT_SERVER=" + doc["mqtt_server"].as<String>() + "\n";
  fileContent += "MQTT_PORT=" + doc["mqtt_port"].as<String>() + "\n";
  fileContent += "MQTT_USER=" + doc["mqtt_user"].as<String>() + "\n";
  fileContent += "MQTT_PASSWORD=" + doc["mqtt_password"].as<String>() + "\n";
  fileContent += "MQTT_CLIENT_ID=" + doc["mqtt_client_id"].as<String>() + "\n\n";

  // Sensor Configuration
  fileContent += "# Sensor Configuration\n";
  fileContent += "VIBRATION_THRESHOLD=" + doc["vibration_threshold"].as<String>() + "\n";

  // Write to file
  File file = LittleFS.open("/env.txt", "w");
  if (!file) {
    Serial.println("Failed to open env.txt for writing");
    return false;
  }

  if (file.print(fileContent)) {
    Serial.println("Configuration saved to env.txt");
    file.close();
    return true;
  } else {
    Serial.println("Failed to write to env.txt");
    file.close();
    return false;
  }
}

void setupWiFi() {
  if (wifi_ssid.length() == 0) {
    Serial.println("WIFI SSID not configured. Please check env.txt");
    return;
  }

  Serial.println("Connecting to WiFi...");
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

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
  if (mqtt_server.length() == 0) {
    Serial.println("MQTT server not configured. Please check env.txt");
    return;
  }

  // Loop until we're reconnected to the MQTT broker
  int attempts = 0;
  while (!mqttClient.connected() && attempts < 5) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect with authentication
    if (mqttClient.connect(mqtt_client_id.c_str(), mqtt_user.c_str(), mqtt_password.c_str())) {
      Serial.println("connected");

      // Subscribe to the sensitivity setting topic
      mqttClient.subscribe(mqtt_topic_sensitivity);

      // Publish initial status
      publishStatus();

      // Publish sensor health status
      const char* health = sensorActive ? "OK" : "FAILED";
      mqttClient.publish(mqtt_topic_sensor_health, health);
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

  // Add route for sensor status
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"boiler\": \"" + String(boilerStatus ? "ON" : "OFF") + "\", ";
    json += "\"sensor\": \"" + String(sensorActive ? "OK" : "FAILED") + "\", ";
    json += "\"vibrationThreshold\": " + String(vibrationThreshold) + ", ";
    json += "\"ip\": \"" + WiFi.localIP().toString() + "\"}";

    request->send(200, "application/json", json);
  });

  // Web configuration - serve HTML file from LittleFS
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(200, "text/plain", "Configuration interface not found. Please upload index.html to LittleFS.");
    }
  });

  server.on("/getconfig", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"wifi_ssid\": \"" + wifi_ssid + "\", ";
    json += "\"wifi_password\": \"" + wifi_password + "\", ";
    json += "\"mqtt_server\": \"" + mqtt_server + "\", ";
    json += "\"mqtt_port\": " + String(mqtt_port) + ", ";
    json += "\"mqtt_user\": \"" + mqtt_user + "\", ";
    json += "\"mqtt_password\": \"" + mqtt_password + "\", ";
    json += "\"mqtt_client_id\": \"" + mqtt_client_id + "\", ";
    json += "\"vibration_threshold\": " + String(vibrationThreshold) + "}";

    request->send(200, "application/json", json);
  });

  server.on("/saveconfig", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"status\":\"error\",\"message\":\"Use JSON body\"}");
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    String json = String((char*)data, len);

    if (saveConfigToEnv(json)) {
      request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration saved\"}");
    } else {
      request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save configuration\"}");
    }
  });

  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Rebooting...\"}");
    // Schedule a reboot after we've sent the response
    delay(500);
    ESP.restart();
  });

  server.begin();

  Serial.println("HTTP server started for OTA updates and configuration");
  Serial.print("Web interface available at http://");
  Serial.println(WiFi.localIP());
}

void checkSensor() {
  // Try to initialize the ADXL345 sensor
  sensorActive = accel.begin();

  if (sensorActive) {
    Serial.println("ADXL345 sensor connected successfully");
    // Set the range to +-2g
    accel.setRange(ADXL345_RANGE_2_G);
  } else {
    Serial.println("Could not find a valid ADXL345 sensor, check wiring!");
  }

  // Publish sensor health status if MQTT is connected
  if (mqttClient.connected()) {
    const char* health = sensorActive ? "OK" : "FAILED";
    mqttClient.publish(mqtt_topic_sensor_health, health);
  }
}

bool detectVibration() {
  if (!sensorActive) {
    return false;
  }

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