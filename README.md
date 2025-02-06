# ESP32-H2 Boiler Vibration Monitoring System

Este proyecto utiliza un ESP32-H2 con un sensor de aceleración ADXL345 para monitorear el estado de una caldera de gasóleo mediante la detección de vibraciones. El sistema se conecta a una red WiFi y a un broker MQTT para reportar el estado y también soporta actualizaciones OTA (Over The Air).

## Características

- Detección de vibración para determinar si la caldera está encendida o apagada
- Sensibilidad ajustable remotamente a través de MQTT
- Conectividad WiFi
- Comunicación MQTT para enviar estados y recibir configuraciones
- Actualizaciones OTA a través de interfaz web
- Monitoreo en tiempo real de niveles de vibración

## Requisitos de Hardware

- ESP32-H2 (compatible con ESP32-H2-DevKitC-1)
- Sensor acelerómetro ADXL345
- Cable micro USB para programación inicial
- Fuente de alimentación para la instalación final

## Conexiones

Conecte el ADXL345 al ESP32-H2 de la siguiente manera:

| ADXL345 | ESP32-H2 |
| ------- | -------- |
| VCC     | 3.3V     |
| GND     | GND      |
| SDA     | GPIO21   |
| SCL     | GPIO22   |

## Configuración

1. Clone este repositorio
2. Abra el proyecto en PlatformIO
3. Edite el archivo `src/main.cpp` para configurar:
   - Credenciales WiFi
   - Configuración del broker MQTT
   - Ajuste la sensibilidad predeterminada según sea necesario

4. Compile y cargue el firmware al ESP32-H2

## Uso de MQTT

El sistema publica en los siguientes tópicos MQTT:

- `boiler/status` - Estado de la caldera ("ON" u "OFF")
- `boiler/vibration` - Valor numérico que indica el nivel de vibración actual

El sistema se suscribe a:

- `boiler/sensitivity/set` - Para ajustar remotamente el umbral de sensibilidad

## Actualización OTA

Una vez que el dispositivo está conectado a la red WiFi, puede actualizar el firmware a través de la interfaz web:

1. Acceda a `http://[IP-DEL-ESP32]/update` en su navegador
2. Siga las instrucciones para cargar el nuevo firmware

## Ajuste de Sensibilidad

El umbral de sensibilidad determina cuánta vibración se requiere para considerar que la caldera está activa. Puede ajustarlo:

1. Localmente cambiando la variable `vibrationThreshold` en el código
2. Remotamente enviando un valor numérico al tópico MQTT `boiler/sensitivity/set`

## Licencia

Este proyecto está disponible bajo la licencia MIT.
