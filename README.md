# ESP32-H2 Boiler Vibration Monitoring System

This project uses an ESP32-H2 with an ADXL345 acceleration sensor to monitor the status of a diesel boiler by detecting vibrations. The system connects to a WiFi network and an MQTT broker to report status and also supports OTA (Over The Air) updates.

## Features

- Vibration detection to determine if the boiler is on or off
- Remotely adjustable sensitivity via MQTT
- WiFi connectivity
- MQTT communication to send status and receive configurations
- OTA updates via web interface
- Real-time monitoring of vibration levels

## Hardware Requirements

- ESP32-H2 (compatible with ESP32-H2-DevKitC-1)
- ADXL345 accelerometer sensor
- Micro USB cable for initial programming
- Power supply for final installation

## Connections

Connect the ADXL345 to the ESP32-H2 as follows:

| ADXL345 | ESP32-H2 |
| ------- | -------- |
| VCC     | 3.3V     |
| GND     | GND      |
| SDA     | GPIO21   |
| SCL     | GPIO22   |

## Configuration

1. Clone this repository
2. Open the project in PlatformIO
3. Edit the `src/main.cpp` file to configure:
   - WiFi credentials
   - MQTT broker configuration
   - Adjust the default sensitivity as needed

4. Compile and upload the firmware to the ESP32-H2

## MQTT Usage

The system publishes to the following MQTT topics:

- `boiler/status` - Boiler status ("ON" or "OFF")
- `boiler/vibration` - Numerical value indicating the current vibration level

The system subscribes to:

- `boiler/sensitivity/set` - To remotely adjust the sensitivity threshold

## OTA Update

Once the device is connected to the WiFi network, you can update the firmware through the web interface:

1. Access `http://[ESP32-IP]/update` in your browser
2. Follow the instructions to upload the new firmware

## Sensitivity Adjustment

The sensitivity threshold determines how much vibration is required to consider the boiler active. You can adjust it:

1. Locally by changing the `vibrationThreshold` variable in the code
2. Remotely by sending a numeric value to the MQTT topic `boiler/sensitivity/set`

## License

This project is available under the MIT license.
