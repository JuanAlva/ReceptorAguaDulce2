# Sistema de Monitoreo de Agua con LoRa y MQTT

Este proyecto implementa un sistema embebido basado en ESP32 para la recepción de datos vía LoRa y su publicación a un broker MQTT usando una conexión Ethernet. Está diseñado para aplicaciones de monitoreo de tanques de agua, específicamente enviando el volumen de agua recibido en m³.

![tanque agua](https://github.com/user-attachments/assets/8808d174-8a91-4f77-a4c3-f475e35f4b97)

## Características

- Recepción de datos mediante LoRa a 433 MHz
- Publicación de datos en un servidor MQTT a través de Ethernet (ENC28J60)
- Análisis de paquetes JSON con datos de volumen (`m3d`)
- Publicación de diagnósticos en caso de error
- Reintentos y reinicio automático del sistema ante fallas recurrentes
- Desactivación de Wi-Fi y Bluetooth para reducir consumo y evitar interferencias
- Watchdog timer implementado para asegurar la estabilidad del sistema

## Diagrama del transmisor

![transmisor drawio](https://github.com/user-attachments/assets/2539979f-c50e-4245-b426-a1e045414f5d)

## Diagrama del recepto

![receptor drawio](https://github.com/user-attachments/assets/91298f90-8119-4a11-b865-e81041f11f87)

## Hardware Requerido

- ESP32
- Módulo LoRa SX1278 (433 MHz)
- Módulo Ethernet ENC28J60
- Fuente de alimentación estable (5V)
- Conexión por SPI para LoRa y Ethernet

## Conexiones LoRa

| Pin ESP32 | Módulo LoRa |
|----------|-------------|
| GPIO 15  | NSS (CS)    |
| GPIO 14  | RESET       |
| GPIO 4   | DIO0 (IRQ)  |
| SPI Pins | MOSI, MISO, SCK (según ESP32) |

## Conexiones Ethernet

- Conexión por SPI usando la librería `EthernetENC`

## Librerías Utilizadas

- `SPI.h`
- `LoRa.h`
- `EthernetENC.h`
- `PubSubClient.h`
- `ArduinoJson.h`
- `WiFi.h`, `esp_bt.h`, `esp_system.h` (para gestión de WiFi, Bluetooth y reinicio)
- `esp_task_wdt.h` (para el Watchdog Timer)

## Configuraciones Importantes

```cpp
// Dirección IP local del dispositivo
IPAddress ip(192, 168, 146, 206);

// Dirección IP del broker MQTT
char server[] = "192.168.252.35";

// Tópicos MQTT
#define CLIENT_ID "publicador-agua"
#define DIAGNOSTIC_TOPIC "TanqueAguaDulce/Diagnostico"
