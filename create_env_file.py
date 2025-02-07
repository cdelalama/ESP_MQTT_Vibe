import os
import sys

def create_env_file():
    # Path to data directory
    data_dir = os.path.join(os.getcwd(), "data")

    # Create data directory if it doesn't exist
    if not os.path.exists(data_dir):
        os.makedirs(data_dir)

    # Path to .env file
    env_file = os.path.join(data_dir, "env.txt")

    # Create .env file if it doesn't exist
    if not os.path.exists(env_file):
        with open(env_file, "w") as f:
            f.write("# WiFi Configuration\n")
            f.write("WIFI_SSID=YOUR_WIFI_SSID\n")
            f.write("WIFI_PASSWORD=YOUR_WIFI_PASSWORD\n\n")
            f.write("# MQTT Configuration\n")
            f.write("MQTT_SERVER=YOUR_MQTT_BROKER_IP\n")
            f.write("MQTT_PORT=1883\n")
            f.write("MQTT_USER=YOUR_MQTT_USERNAME\n")
            f.write("MQTT_PASSWORD=YOUR_MQTT_PASSWORD\n")
            f.write("MQTT_CLIENT_ID=ESP32H2_Boiler\n\n")
            f.write("# Sensor Configuration\n")
            f.write("VIBRATION_THRESHOLD=1.0\n")

        print("Default env.txt file created in /data directory")
    else:
        print("env.txt file already exists in /data directory")

# Run the function
create_env_file()

# Continue with normal build process
env = DefaultEnvironment()