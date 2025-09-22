#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

// ═══════════════════════════════════════════════════════════════════════════════
//                              EXTERNE ABHÄNGIGKEITEN
// ═══════════════════════════════════════════════════════════════════════════════

// Externe Objekte (definiert in main.cpp)
extern WiFiClient espClient;
extern PubSubClient client;
extern SystemStatus systemStatus;
extern RenderManager renderManager;
extern SensorData sensors[];
extern float stockReference;
extern float stockPreviousClose;

// Power Management Variablen
extern float pvPower;
extern float gridPower;
extern float loadPower;
extern float storagePower;

// Richtungsinformationen
extern bool isGridFeedIn;
extern bool isStorageCharging;
extern unsigned long lastWifiReconnect;

// Struktur für letzte gültige Power-Werte
struct LastValidPowerValues {
  float pvPower;
  float gridPower;
  float loadPower;
  float storagePower;
  unsigned long pvPowerTime;
  unsigned long gridPowerTime;
  unsigned long loadPowerTime;
  unsigned long storagePowerTime;
};
extern LastValidPowerValues lastValidPower;

// ═══════════════════════════════════════════════════════════════════════════════
//                              NETZWERK-FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

// WiFi-Management
void connectWiFi();
void reconnectWiFi();
bool checkWifiConnection();

// MQTT-Management
void reconnectMQTT();
void onMqttMessage(char* topic, byte* payload, unsigned int length);
void processMqttMessage(const char* topic, const String& message);

// Sensor-Datenverarbeitung (aus MQTT)
void updateSensorValue(int index, float newValue);
void processDayAheadPriceData(const String& message);

// Hilfsfunktionen
bool isValidSensorIndex(int index);
void updatePVNetDisplay();

#endif // NETWORK_H