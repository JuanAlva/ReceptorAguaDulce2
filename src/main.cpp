#include <SPI.h>
#include <LoRa.h>
#include <EthernetENC.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "esp_task_wdt.h" // Biblioteca para Watchdog
#include <WiFi.h>
#include <esp_bt.h>
#include <esp_system.h>  // Incluir esta librería para utilizar esp_restart()

// Configuración Ethernet y MQTT
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 146, 206);
IPAddress myDns(192, 168, 146, 1);
char server[] = "192.168.252.35"; // Dirección del broker MQTT
#define CLIENT_ID "publicador-agua"
//#define INTERVAL 5000 // Intervalo de publicación en ms
#define DIAGNOSTIC_TOPIC "TanqueAguaDulce/Diagnostico"

EthernetClient client;
PubSubClient mqttClient(client);
unsigned long previousMillis = 0;

// Configuración LoRa
const int csPin = 15;          
const int resetPin = 14;       
const int irqPin = 4;          

// Variables para monitoreo y errores
float receivedM3D = 0.0;   // Metros cúbicos
unsigned long lastLoRaReceivedTime = 0; // Último tiempo en el que se recibieron datos
#define LORA_TIMEOUT 5000 // Tiempo máximo sin datos (en ms)

#define WDT_TIMEOUT 20 // Tiempo en segundos

// Variables para control de errores persistentes
int loraErrorCount = 0;
int mqttErrorCount = 0;
const int MAX_ERROR_COUNT = 5; // Umbral de errores antes de reiniciar

// Declaraciones de funciones
void receiveLoRaData();
void sendData();
void reconnectMQTT();
bool resetLoRa();
void reportError(const char* errorMessage);
void checkSystemHealth();

void disableWiFiAndBluetooth() {
    WiFi.mode(WIFI_OFF);   // Apaga Wi-Fi
    WiFi.disconnect(true); // Desconecta Wi-Fi
    btStop();              // Apaga Bluetooth
}

void initLoRa() {
  // Configuración LoRa
  Serial.println("Inicializando LoRa receptor...");
  LoRa.setPins(csPin, resetPin, irqPin);
  if (!LoRa.begin(433E6)) {
    Serial.println("Error al inicializar LoRa!");
    reportError("Error crítico: fallo inicial en LoRa.");
    while (1); // Detener el sistema si no se puede inicializar LoRa
  }
  LoRa.setSyncWord(0xF3);
  LoRa.setTxPower(20, PA_OUTPUT_PA_BOOST_PIN); // Potencia de transmisión
  LoRa.setSpreadingFactor(7);  // Spreading Factor
  LoRa.setSignalBandwidth(125E3); // Ancho de banda (125 kHz)
  LoRa.setCodingRate4(5);  // Coding Rate 4/5
  Serial.println("LoRa inicializado con éxito.");
}

void receiveLoRaData() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    lastLoRaReceivedTime = millis(); // Actualizar el tiempo de recepción
    String receivedData = "";
    
    while (LoRa.available()) {
      receivedData += (char)LoRa.read();
    }

    Serial.print("Datos recibidos: ");
    Serial.println(receivedData);

    // Procesar JSON
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, receivedData);

    if (!error) {
      if (doc.containsKey("m3d")) {
          receivedM3D = doc["m3d"].as<float>();
          Serial.printf("Metros cúbicos recibidos: %.2f m3\n", receivedM3D);
          sendData(); // Publicar datos
      } else {
          Serial.println("El campo 'm3' no existe en el JSON recibido.");
      }
    } else {
      Serial.print("Error al deserializar JSON: ");
      Serial.println(error.c_str());
    }
  }
}

void sendData() {
  char buffer[10];
  // Publicar metros cúbicos
  snprintf(buffer, sizeof(buffer), "%.3f", receivedM3D);
  if (mqttClient.publish("TanqueAguaDulce/M3D", buffer)) {
      Serial.printf("Metros cúbicos publicados: %s m3\n", buffer);
  } else {
      Serial.println("Error al publicar metros cúbicos.");
      mqttErrorCount++;
  }
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.println("Intentando conectar a MQTT...");
    if (mqttClient.connect(CLIENT_ID)) {
      Serial.println("Conexión MQTT exitosa.");
      mqttErrorCount = 0; // Reiniciar contador si tiene éxito
    } else {
      mqttErrorCount++; // Incrementar contador de errores
      Serial.print("Error al conectar MQTT: ");
      Serial.println(mqttClient.state());
      delay(5000); // Esperar antes de reintentar
      if (mqttErrorCount >= MAX_ERROR_COUNT) {
        break; // Salir del bucle si se alcanzó el máximo de errores
      }
    }
  }
}

bool resetLoRa() {
  LoRa.end(); // Apagar LoRa
  delay(100);
  if (LoRa.begin(433E6)) {
    LoRa.setSyncWord(0xF3);
    LoRa.setTxPower(20, PA_OUTPUT_PA_BOOST_PIN); // Potencia de transmisión
    LoRa.setSpreadingFactor(7);  // Spreading Factor
    LoRa.setSignalBandwidth(125E3); // Ancho de banda (125 kHz)
    LoRa.setCodingRate4(5);  // Coding Rate 4/5
    loraErrorCount = 0; // Reiniciar contador si tiene éxito
    Serial.println("LoRa reiniciado con éxito.");
    return true;
  } else {
    loraErrorCount++; // Incrementar contador de errores
    Serial.println("Error al reiniciar LoRa.");
    return false;
  }
}

void reportError(const char* errorMessage) {
  if (mqttClient.connected()) {
    mqttClient.publish(DIAGNOSTIC_TOPIC, errorMessage);
    Serial.printf("Error reportado: %s\n", errorMessage);
  } else {
    Serial.println("No se pudo reportar el error: MQTT no conectado.");
  }
}

void checkSystemHealth() {
  if (loraErrorCount >= MAX_ERROR_COUNT || mqttErrorCount >= MAX_ERROR_COUNT) {
    Serial.println("Umbral de errores alcanzado. Reiniciando el ESP32...");
    ESP.restart(); // Reiniciar el ESP32
  }
}

void setup() {
  Serial.begin(115200);
  esp_task_wdt_init(WDT_TIMEOUT, true);  // Inicializar el Watchdog
  esp_task_wdt_add(NULL);                // Agregar la tarea principal al Watchdog
  while (!Serial);

  disableWiFiAndBluetooth();
  
  // Configuración Ethernet
  Ethernet.begin(mac, ip, myDns);
  Serial.print("IP asignada: ");
  Serial.println(Ethernet.localIP());

  // Configuración MQTT
  mqttClient.setServer(server, 1883);
  mqttClient.setSocketTimeout(5); // Establecer un tiempo de espera de 5 segundos
  
  initLoRa();

  reconnectMQTT();
}

void loop() {
  // Verificar salud del sistema
  checkSystemHealth();

  // Reconectar MQTT si es necesario
  if (!mqttClient.connected()) {
    Serial.println("MQTT desconectado, intentando reconectar...");
    reconnectMQTT();
  }

  mqttClient.loop(); // Mantener la conexión con el broker MQTT
  receiveLoRaData();

  // Verificar si el módulo LoRa ha dejado de recibir datos
  if (millis() - lastLoRaReceivedTime > LORA_TIMEOUT) {
    Serial.println("Tiempo de espera LoRa excedido. Reiniciando LoRa...");
    reportError("Advertencia: reinicio del módulo LoRa por tiempo excedido.");
    if (!resetLoRa()) {
      Serial.println("Error crítico: no se pudo reiniciar LoRa.");
      reportError("Error crítico: fallo en reinicio de LoRa.");
      return;
    }
    lastLoRaReceivedTime = millis(); // Reiniciar el contador
  }

  esp_task_wdt_reset(); // Alimenta el Watchdog
}